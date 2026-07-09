/* SPDX-License-Identifier: MIT */
#include "lower.h"
#include "target.h"
#include "diag.h"
#include "arena.h"

#include <assert.h>
#include <limits.h>

typedef struct {
    LirFn *lf;
    Function *fn;
    int ret_label;
} LowerCtx;

static int emit(LowerCtx *c, Instr ins)
{
    return lir_emit(c->lf, ins);
}

static int fresh(LowerCtx *c)
{
    return lir_new_vreg(c->lf);
}

static int vreg_home_offset(const LirFn *lf, int vreg)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].vreg == vreg)
            return lf->homes[i].offset;
    }
    return INT_MIN;
}

static int assigns_to_offset(Node *n, int offset)
{
    if (!n)
        return 0;

    switch (n->kind) {
    case ND_ASSIGN:
        if (n->lhs->kind == ND_VAR && n->lhs->offset == offset)
            return 1;
        return assigns_to_offset(n->lhs, offset) ||
               assigns_to_offset(n->rhs, offset);
    case ND_BINOP:
        return assigns_to_offset(n->lhs, offset) ||
               assigns_to_offset(n->rhs, offset);
    case ND_INIT_LIST:
        for (Node *e = n->body; e; e = e->next) {
            if (assigns_to_offset(e, offset))
                return 1;
        }
        return 0;
    case ND_CALL:
        for (Node *a = n->args; a; a = a->next) {
            if (assigns_to_offset(a, offset))
                return 1;
        }
        return 0;
    case ND_NEG:
    case ND_CAST:
    case ND_DEREF:
        return assigns_to_offset(n->operand, offset);
    case ND_EXPR_STMT:
        return assigns_to_offset(n->operand, offset);
    case ND_IF:
        return assigns_to_offset(n->cond, offset) ||
               assigns_to_offset(n->then_body, offset) ||
               assigns_to_offset(n->else_body, offset);
    case ND_WHILE:
    case ND_FOR:
        return assigns_to_offset(n->cond, offset) ||
               assigns_to_offset(n->init, offset) ||
               assigns_to_offset(n->step, offset) ||
               assigns_to_offset(n->then_body, offset);
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next) {
            if (assigns_to_offset(s, offset))
                return 1;
        }
        return 0;
    default:
        return 0;
    }
}

static int snapshot_home(LowerCtx *c, int v)
{
    int copy = fresh(c);
    emit(c, (Instr){ .op = LIR_MOV, .dst = copy, .a = lir_vreg(v) });
    return copy;
}

static int protect_home_before(LowerCtx *c, int v, Node *later)
{
    if (!lir_is_home_vreg(c->lf, v))
        return v;
    int off = vreg_home_offset(c->lf, v);
    if (off == INT_MIN || !assigns_to_offset(later, off))
        return v;
    return snapshot_home(c, v);
}

static void emit_load_slot(LowerCtx *c, int dst, Type *ty, int offset);
static void emit_store_slot(LowerCtx *c, int src, Type *ty, int offset);
static void lower_bitfield_store_off(LowerCtx *c, int struct_off, Member *m, int val);
static void lower_cast_into(LowerCtx *c, int dst, Node *n);

static int offset_address_taken(Function *fn, int offset)
{
    for (int i = 0; i < fn->nframe_locals; i++) {
        if (fn->frame_locals[i].offset == offset)
            return fn->frame_locals[i].address_taken;
    }
    return 0;
}

static int can_promote(Function *fn, Type *ty, int offset)
{
    if (!type_is_scalar(ty) || type_is_array(ty))
        return 0;
    return !offset_address_taken(fn, offset);
}

static void lower_params(LowerCtx *c)
{
    int i = 0;
    for (Param *p = c->fn->params; p; p = p->next, i++) {
        Type *pty = type_decay(p->ty);
        if (!can_promote(c->fn, pty, p->offset))
            continue;
        int v = fresh(c);
        lir_bind_home(c->lf, p->offset, v);
        if (i < X86_SYSV.nargs_reg)
            emit(c, (Instr){
                .op = LIR_MOV, .dst = v, .a = lir_phys(X86_SYSV.arg_regs[i]) });
        else
            emit_load_slot(c, v, pty, p->offset);
    }
}

static int is_cmp(BinOp op)
{
    return op == OP_EQ || op == OP_NE ||
           op == OP_LT || op == OP_LE ||
           op == OP_GT || op == OP_GE;
}

static LirCond binop_cc(BinOp op)
{
    switch (op) {
    case OP_EQ: return CC_EQ;
    case OP_NE: return CC_NE;
    case OP_LT: return CC_LT;
    case OP_LE: return CC_LE;
    case OP_GT: return CC_GT;
    case OP_GE: return CC_GE;
    default:
        assert(0);
        return CC_EQ;
    }
}

static LirCond false_branch_cc(BinOp op)
{
    switch (op) {
    case OP_LT: return CC_GE;
    case OP_LE: return CC_GT;
    case OP_GT: return CC_LE;
    case OP_GE: return CC_LT;
    case OP_EQ: return CC_NE;
    case OP_NE: return CC_EQ;
    default:
        assert(0);
        return CC_EQ;
    }
}

static int binop_width(Node *lhs, Node *rhs)
{
    TypeScalarInfo li;
    TypeScalarInfo ri;

    if (type_scalar_info(lhs->ty, &li) && li.width == 8)
        return LIR_W8;
    if (type_scalar_info(rhs->ty, &ri) && ri.width == 8)
        return LIR_W8;
    return LIR_W4;
}

static LirWidth expr_width(Node *n)
{
    TypeScalarInfo si;

    if (!n || !n->ty)
        return LIR_W4;
    if (type_scalar_info(n->ty, &si) && si.width == 8)
        return LIR_W8;
    return LIR_W4;
}

static int store_width_bytes(Type *ty)
{
    TypeScalarInfo si;

    if (type_scalar_info(ty, &si))
        return si.width;
    return 0;
}

static LirWidth store_lir_width(Type *ty)
{
    int w = store_width_bytes(ty);
    return w == 8 ? LIR_W8 : LIR_W4;
}

static LirSign load_sign(Type *ty)
{
    TypeScalarInfo si;

    if (type_scalar_info(ty, &si) && si.is_integer && !si.is_signed)
        return LIR_SGN_Z;
    return LIR_SGN_S;
}

static LirSign arith_sign(Type *ty)
{
    TypeScalarInfo si;

    if (type_scalar_info(ty, &si) && si.is_integer && !si.is_signed)
        return LIR_SGN_U;
    return LIR_SGN_S;
}

static LirSign binop_sign(Node *lhs, Node *rhs, Type *res_ty, BinOp op)
{
    if (is_cmp(op))
        return arith_sign(type_arith_convert(lhs->ty, rhs->ty));
    return arith_sign(res_ty);
}

static LirWidth load_lir_width(Type *ty)
{
    TypeScalarInfo si;

    if (type_scalar_info(ty, &si) && si.width == 8)
        return LIR_W8;
    return LIR_W4;
}

static void emit_widen_to_rax(LowerCtx *c, int v, Type *ty)
{
    if (load_lir_width(ty) == LIR_W8)
        return;
    if (type_is_char(ty))
        return;
    if (type_is_short(ty) && load_sign(ty) == LIR_SGN_S) {
        int t = fresh(c);
        emit(c, (Instr){
            .op = LIR_CONV, .dst = t, .a = lir_vreg(v), .conv = CONV_SEXT16 });
        emit(c, (Instr){ .op = LIR_MOV, .dst = v, .a = lir_vreg(t) });
    }
    emit(c, (Instr){
        .op = LIR_CONV, .dst = v, .a = lir_vreg(v), .conv = CONV_SEXT32_64 });
}

static void emit_load_slot(LowerCtx *c, int dst, Type *ty, int offset)
{
    int bytes = store_width_bytes(ty);
    emit(c, (Instr){
        .op = LIR_LOAD,
        .dst = dst,
        .a = lir_mem(LIR_FP, offset),
        .w = load_lir_width(ty),
        .sgn = load_sign(ty),
        .aux = bytes,
    });
    emit_widen_to_rax(c, dst, ty);
}

static void emit_store_slot(LowerCtx *c, int src, Type *ty, int offset)
{
    emit(c, (Instr){
        .op = LIR_STORE,
        .a = lir_mem(LIR_FP, offset),
        .b = lir_vreg(src),
        .w = store_lir_width(ty),
        .aux = store_width_bytes(ty),
    });
}

static void emit_memcpy(LowerCtx *c, int dst, int src, int size)
{
    emit(c, (Instr){
        .op = LIR_MEMCPY,
        .a = lir_vreg(dst),
        .b = lir_vreg(src),
        .aux = size,
    });
}

static int lower_object_addr(LowerCtx *c, Node *n);

static int ptr_elem_size(Type *ptr_ty)
{
    Type *elem = type_ptr_elem(ptr_ty);
    return elem ? type_size(elem) : 1;
}

/* x86-64 add/sub/cmp/imul immediates are sign-extended imm32, so a constant
   may be used in place only when it fits a signed 32-bit field. */
static int fits_imm32(long v)
{
    return v >= -2147483647L - 1 && v <= 2147483647L;
}

static int is_ptr_int_arith(BinOp op, Node *lhs, Node *rhs)
{
    if (op == OP_ADD)
        return (type_is_pointer(lhs->ty) && type_is_integer(rhs->ty)) ||
               (type_is_integer(lhs->ty) && type_is_pointer(rhs->ty));
    if (op == OP_SUB)
        return type_is_pointer(lhs->ty) && type_is_integer(rhs->ty);
    return 0;
}

static int lower_addr(LowerCtx *c, Node *n);
static int lower_expr(LowerCtx *c, Node *n);
static int lower_member_addr(LowerCtx *c, Node *n);

static void lower_init_from_type(LowerCtx *c, Type *ty, Node **pcursor, int base_off)
{
    int i;

    if (type_is_array(ty)) {
        Type *elem = type_array_elem(ty);
        int esz = type_size(elem);
        int n = type_array_count(ty);

        for (i = 0; i < n; i++)
            lower_init_from_type(c, elem, pcursor, base_off + i * esz);
        return;
    }
    if (type_is_struct(ty)) {
        for (i = 0; i < ty->nmembers; i++) {
            Member *m = &ty->members[i];

            if (m->is_bitfield && m->bit_width > 0) {
                Node *e = *pcursor;
                int v;

                assert(e);
                *pcursor = e->next;
                v = lower_expr(c, e);
                lower_bitfield_store_off(c, base_off, m, v);
            } else {
                lower_init_from_type(c, m->ty, pcursor, base_off + m->offset);
            }
        }
        return;
    }
    if (type_is_union(ty)) {
        if (ty->nmembers > 0)
            lower_init_from_type(c, ty->members[0].ty, pcursor, base_off);
        return;
    }
    {
        Node *e = *pcursor;
        int v;

        assert(e);
        *pcursor = e->next;
        v = lower_expr(c, e);
        emit_store_slot(c, v, ty, base_off);
    }
}

static void lower_brace_init(LowerCtx *c, Node *decl)
{
    Node *cursor = decl->init->body;

    lower_init_from_type(c, decl->ty, &cursor, decl->offset);
}

static int lower_object_addr(LowerCtx *c, Node *n)
{
    switch (n->kind) {
    case ND_VAR: {
        int dst = fresh(c);
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem(LIR_FP, n->offset),
        });
        return dst;
    }
    case ND_DEREF:
        return lower_expr(c, n->operand);
    case ND_MEMBER:
        return lower_member_addr(c, n);
    default:
        assert(0 && "invalid object address");
        return fresh(c);
    }
}

static void lower_stmt(LowerCtx *c, Node *n);

static Member *member_meta(Node *n)
{
    return &n->lhs->ty->members[n->member_index];
}

static int emit_binop_imm(LowerCtx *c, LirOp op, int dst, int lhs, long imm)
{
    int t = fresh(c);
    emit(c, (Instr){
        .op = op, .dst = t, .a = lir_vreg(lhs), .b = lir_imm(imm), .w = LIR_W8 });
    if (t != dst)
        emit(c, (Instr){ .op = LIR_MOV, .dst = dst, .a = lir_vreg(t) });
    return dst;
}

static int lower_bitfield_unit_load_off(LowerCtx *c, int struct_off, int unit_off)
{
    int dst = fresh(c);

    emit(c, (Instr){
        .op = LIR_LOAD,
        .dst = dst,
        .a = lir_mem(LIR_FP, struct_off + unit_off),
        .w = LIR_W4,
        .sgn = LIR_SGN_Z,
        .aux = 4,
    });
    emit(c, (Instr){
        .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_ZEXT32 });
    return dst;
}

static void lower_bitfield_unit_store_off(LowerCtx *c, int struct_off, int unit_off, int val)
{
    int t = fresh(c);
    emit(c, (Instr){
        .op = LIR_CONV, .dst = t, .a = lir_vreg(val), .conv = CONV_TRUNC_LO32 });
    emit(c, (Instr){
        .op = LIR_STORE,
        .a = lir_mem(LIR_FP, struct_off + unit_off),
        .b = lir_vreg(t),
        .w = LIR_W4,
        .aux = 4,
    });
}

static void lower_bitfield_store_off(LowerCtx *c, int struct_off, Member *m, int val)
{
    int unit = lower_bitfield_unit_load_off(c, struct_off, m->offset);
    long bmask = ((1L << m->bit_width) - 1) << m->bit_offset;
    int cleared = fresh(c);
    int bits = fresh(c);
    int shifted = fresh(c);
    int merged = fresh(c);

    emit_binop_imm(c, LIR_AND, cleared, unit, ~bmask);
    emit_binop_imm(c, LIR_AND, bits, val, (1L << m->bit_width) - 1);
    emit_binop_imm(c, LIR_SHL, shifted, bits, m->bit_offset);
    emit(c, (Instr){
        .op = LIR_OR, .dst = merged, .a = lir_vreg(cleared), .b = lir_vreg(shifted),
        .w = LIR_W8 });
    lower_bitfield_unit_store_off(c, struct_off, m->offset, merged);
}

static int lower_bitfield_unit_load(LowerCtx *c, Node *base, int unit_off)
{
    if (base->kind == ND_VAR && !base->var_decay)
        return lower_bitfield_unit_load_off(c, base->offset, unit_off);

    {
        int dst = fresh(c);
        int b = lower_object_addr(c, base);

        emit(c, (Instr){
            .op = LIR_LOAD,
            .dst = dst,
            .a = lir_mem(b, unit_off),
            .w = LIR_W4,
            .sgn = LIR_SGN_Z,
            .aux = 4,
        });
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_ZEXT32 });
        return dst;
    }
}

static void lower_bitfield_unit_store(LowerCtx *c, Node *base, int unit_off, int val)
{
    if (base->kind == ND_VAR && !base->var_decay) {
        lower_bitfield_unit_store_off(c, base->offset, unit_off, val);
        return;
    }

    {
        int t = fresh(c);
        int b = lower_object_addr(c, base);

        emit(c, (Instr){
            .op = LIR_CONV, .dst = t, .a = lir_vreg(val), .conv = CONV_TRUNC_LO32 });
        emit(c, (Instr){
            .op = LIR_STORE,
            .a = lir_mem(b, unit_off),
            .b = lir_vreg(t),
            .w = LIR_W4,
            .aux = 4,
        });
    }
}

static int lower_bitfield_load(LowerCtx *c, Node *n, Member *m)
{
    int unit = lower_bitfield_unit_load(c, n->lhs, m->offset);
    int dst = fresh(c);
    int t = fresh(c);
    long mask = (1L << m->bit_width) - 1;

    emit_binop_imm(c, LIR_SHR, t, unit, m->bit_offset);
    emit_binop_imm(c, LIR_AND, dst, t, mask);
    if (type_is_signed(m->ty) && !type_is_unsigned(m->ty)) {
        int sh = 64 - m->bit_width;
        emit_binop_imm(c, LIR_SHL, dst, dst, sh);
        emit_binop_imm(c, LIR_SAR, dst, dst, sh);
    }
    return dst;
}

static void lower_bitfield_store(LowerCtx *c, Node *lhs, Member *m, int val)
{
    if (lhs->lhs->kind == ND_VAR && !lhs->lhs->var_decay) {
        lower_bitfield_store_off(c, lhs->lhs->offset, m, val);
        return;
    }

    {
        int unit = lower_bitfield_unit_load(c, lhs->lhs, m->offset);
        long bmask = ((1L << m->bit_width) - 1) << m->bit_offset;
        int cleared = fresh(c);
        int bits = fresh(c);
        int shifted = fresh(c);
        int merged = fresh(c);

        emit_binop_imm(c, LIR_AND, cleared, unit, ~bmask);
        emit_binop_imm(c, LIR_AND, bits, val, (1L << m->bit_width) - 1);
        emit_binop_imm(c, LIR_SHL, shifted, bits, m->bit_offset);
        emit(c, (Instr){
            .op = LIR_OR, .dst = merged, .a = lir_vreg(cleared), .b = lir_vreg(shifted),
            .w = LIR_W8 });
        lower_bitfield_unit_store(c, lhs->lhs, m->offset, merged);
    }
}

static int lower_member_addr(LowerCtx *c, Node *n)
{
    Type *sty = n->lhs->ty;
    Member *m = &sty->members[n->member_index];
    int dst = fresh(c);

    if (n->lhs->kind == ND_VAR && !n->lhs->var_decay) {
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem(LIR_FP, n->lhs->offset + m->offset),
        });
        return dst;
    }

    {
        int base = lower_addr(c, n->lhs);
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem(base, m->offset),
        });
        return dst;
    }
}

static int lower_addr(LowerCtx *c, Node *n)
{
    switch (n->kind) {
    case ND_VAR:
        if (n->var_decay) {
            int dst = fresh(c);
            emit(c, (Instr){
                .op = LIR_LEA,
                .dst = dst,
                .a = lir_mem(LIR_FP, n->offset),
            });
            return dst;
        }
        {
            int dst = fresh(c);
            emit_load_slot(c, dst, n->ty, n->offset);
            return dst;
        }
    case ND_DEREF:
        return lower_expr(c, n->operand);
    case ND_MEMBER:
        return lower_member_addr(c, n);
    default:
        assert(0 && "invalid lvalue");
        return fresh(c);
    }
}

static void emit_conv(LowerCtx *c, int dst, ConvKind k)
{
    emit(c, (Instr){ .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = k });
}

/*
 * Canonical integer cast (C89 6.2.1.2 conversions).
 *
 * A value of type T is `value mod 2^(width_of_T)` reinterpreted with T's
 * signedness.  Operationally this is a single decision keyed on the
 * destination width/signedness (for narrowing or same-width casts) or on the
 * source signedness (when widening to 64 bits), which is what this function
 * encodes as one table instead of the previous scattered special cases.
 *
 * CONV op reach reminder: ZEXT8/ZEXT16/ZEXT32/SEXT16 already extend to the full
 * 64-bit register; SEXT8 and SEXT32_64 only reach 32 and 64 respectively, so a
 * signed byte widened to 64 bits is composed as SEXT8 then SEXT32_64.
 */
static void lower_mem_zero(LowerCtx *c, int addr, int size)
{
    int z = fresh(c);
    int off;

    emit(c, (Instr){ .op = LIR_MOVI, .dst = z, .a = lir_imm(0) });
    for (off = 0; off < size; off += 8) {
        int n = size - off;
        LirWidth w;

        if (n > 8)
            n = 8;
        if (n >= 8)
            w = LIR_W8;
        else
            w = LIR_W4;
        emit(c, (Instr){
            .op = LIR_STORE,
            .a = lir_mem(addr, off),
            .b = lir_vreg(z),
            .w = w,
            .aux = n,
        });
    }
}

static int is_struct_scalar_cast(Node *n)
{
    return n && n->kind == ND_CAST && type_is_struct(n->ty) && n->operand &&
           type_is_scalar(n->operand->ty) && type_cast_target_ok(n->ty);
}

static void lower_struct_from_scalar(LowerCtx *c, int addr, Type *sty, Node *scalar)
{
    int sz = type_size(sty);
    int sw = store_width_bytes(scalar->ty);
    int store_bytes = sw < sz ? sw : sz;
    int val = lower_expr(c, scalar);
    int t = fresh(c);
    Node widen = {0};

    if (sz > store_bytes)
        lower_mem_zero(c, addr, sz);

    emit(c, (Instr){ .op = LIR_MOV, .dst = t, .a = lir_vreg(val) });
    widen.kind = ND_CAST;
    widen.ty = type_long();
    widen.cast_ty = type_long();
    widen.operand = scalar;
    widen.loc = scalar->loc;
    lower_cast_into(c, t, &widen);
    emit(c, (Instr){
        .op = LIR_STORE,
        .a = lir_mem(addr, 0),
        .b = lir_vreg(t),
        .w = store_lir_width(scalar->ty),
        .aux = store_bytes,
    });
}

static void lower_cast_into(LowerCtx *c, int dst, Node *n)
{
    int v = lower_expr(c, n->operand);
    Type *to = n->ty;
    Type *from = n->operand->ty;

    emit(c, (Instr){ .op = LIR_MOV, .dst = dst, .a = lir_vreg(v) });

    if (type_is_void(to))
        return;

    TypeScalarInfo di;
    TypeScalarInfo si;
    if (!type_scalar_info(to, &di) || !type_scalar_info(from, &si))
        return;

    int dw = di.width;
    int ds = di.is_signed;
    int sw = si.width;
    int ss = si.is_signed;

    /* No representation change. */
    if (dw == sw && ds == ss)
        return;

    if (dw >= 8) {
        /* Widen to 64 bits preserving the source value (per source sign). */
        if (sw >= 8)
            return;
        if (sw == 4) {
            emit_conv(c, dst, ss ? CONV_SEXT32_64 : CONV_ZEXT32);
        } else if (sw == 2) {
            emit_conv(c, dst, ss ? CONV_SEXT16 : CONV_ZEXT16);
        } else {
            if (ss) {
                emit_conv(c, dst, CONV_SEXT8);
                emit_conv(c, dst, CONV_SEXT32_64);
            } else {
                emit_conv(c, dst, CONV_ZEXT8);
            }
        }
        return;
    }

    if (dw == 4) {
        /* Truncate to 32 bits then re-canonicalise per destination sign.
           A signed int from a <=32-bit source is already canonical. */
        if (sw >= 8)
            emit_conv(c, dst, ds ? CONV_SEXT32_64 : CONV_ZEXT32);
        else if (ds && sw == 4)
            emit_conv(c, dst, CONV_SEXT32_64);
        else if (!ds)
            emit_conv(c, dst, CONV_ZEXT32);
        return;
    }

    if (dw == 2) {
        emit_conv(c, dst, ds ? CONV_SEXT16 : CONV_ZEXT16);
        return;
    }

    /* dw == 1 */
    emit_conv(c, dst, ds ? CONV_SEXT8 : CONV_ZEXT8);
}

static void lower_ptr_diff(LowerCtx *c, int dst, Node *lhs, Node *rhs)
{
    int scale = ptr_elem_size(lhs->ty);
    int vr = lower_expr(c, rhs);
    int vl = lower_expr(c, lhs);
    int t = fresh(c);

    emit(c, (Instr){
        .op = LIR_SUB, .dst = t, .a = lir_vreg(vl), .b = lir_vreg(vr), .w = LIR_W8 });
    if (scale > 1) {
        int vs = fresh(c);
        emit(c, (Instr){ .op = LIR_MOVI, .dst = vs, .a = lir_imm(scale) });
        emit(c, (Instr){
            .op = LIR_DIV, .dst = dst, .a = lir_vreg(t), .b = lir_vreg(vs), .w = LIR_W8 });
    } else {
        emit(c, (Instr){ .op = LIR_MOV, .dst = dst, .a = lir_vreg(t) });
    }
}

static void lower_ptr_int_arith(LowerCtx *c, int dst, BinOp op,
                                Node *lhs, Node *rhs)
{
    Node *ptr = type_is_pointer(lhs->ty) ? lhs : rhs;
    Node *idx = type_is_pointer(lhs->ty) ? rhs : lhs;
    int scale = ptr_elem_size(ptr->ty);
    int vi = lower_expr(c, idx);
    int vp = lower_expr(c, ptr);

    if (scale > 1) {
        int t = fresh(c);
        emit(c, (Instr){
            .op = LIR_MUL, .dst = t, .a = lir_vreg(vi), .b = lir_imm(scale), .w = LIR_W8 });
        vi = t;
    }

    if (op == OP_ADD) {
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem_idx(vp, vi, 1, 0),
        });
    } else {
        emit(c, (Instr){
            .op = LIR_SUB, .dst = dst, .a = lir_vreg(vp), .b = lir_vreg(vi), .w = LIR_W8 });
    }
}

static void lower_setcc(LowerCtx *c, int dst, BinOp op, int lhs, Operand rhs,
                        int w, LirSign sgn)
{
    emit(c, (Instr){
        .op = LIR_SETCC,
        .dst = dst,
        .a = lir_vreg(lhs),
        .b = rhs,
        .w = (LirWidth)w,
        .cc = binop_cc(op),
        .sgn = sgn,
    });
}

static void lower_binop(LowerCtx *c, int dst, BinOp op, Node *lhs, Node *rhs,
                        Type *res_ty)
{
    if (type_is_pointer(lhs->ty) && type_is_pointer(rhs->ty) && op == OP_SUB) {
        lower_ptr_diff(c, dst, lhs, rhs);
        return;
    }

    if (is_ptr_int_arith(op, lhs, rhs)) {
        lower_ptr_int_arith(c, dst, op, lhs, rhs);
        return;
    }

    int w = binop_width(lhs, rhs);
    int vl = lower_expr(c, lhs);
    vl = protect_home_before(c, vl, rhs);
    LirSign sgn = binop_sign(lhs, rhs, res_ty, op);

    /* Fold a constant right operand into an immediate for add/sub/mul and the
       comparisons; div/mod need the divisor in a register, so keep those in a
       vreg. */
    int imm_ok = rhs->kind == ND_NUM && fits_imm32(rhs->val) &&
                 op != OP_DIV && op != OP_MOD;
    Operand rb;
    if (imm_ok)
        rb = lir_imm(rhs->val);
    else
        rb = lir_vreg(lower_expr(c, rhs));

    switch (op) {
    case OP_ADD:
        emit(c, (Instr){
            .op = LIR_ADD, .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w });
        return;
    case OP_SUB:
        emit(c, (Instr){
            .op = LIR_SUB, .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w });
        return;
    case OP_MUL:
        emit(c, (Instr){
            .op = LIR_MUL, .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w });
        return;
    case OP_DIV:
        emit(c, (Instr){
            .op = LIR_DIV, .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w, .sgn = sgn });
        return;
    case OP_MOD:
        emit(c, (Instr){
            .op = LIR_MOD, .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w, .sgn = sgn });
        return;
    case OP_EQ:
    case OP_NE:
    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
        lower_setcc(c, dst, op, vl, rb, w, sgn);
        return;
    case OP_COMMA:
        (void)lower_expr(c, lhs);
        if (imm_ok)
            emit(c, (Instr){
                .op = LIR_MOVI, .dst = dst, .a = lir_imm(rhs->val) });
        else {
            int vr = lower_expr(c, rhs);
            emit(c, (Instr){
                .op = LIR_MOV, .dst = dst, .a = lir_vreg(vr) });
        }
        return;
    }
}

static int lower_call(LowerCtx *c, Node *n)
{
    if (n->nargs > XCC_MAX_CALL_ARGS)
        diag_fatal("internal error: too many call arguments");

    Operand *args = arena_alloc((size_t)n->nargs * sizeof(*args));
    int i = 0;
    for (Node *a = n->args; a; a = a->next, i++) {
        int v = lower_expr(c, a);
        if (lir_is_home_vreg(c->lf, v)) {
            int off = vreg_home_offset(c->lf, v);
            for (Node *b = a->next; b; b = b->next) {
                if (assigns_to_offset(b, off)) {
                    v = snapshot_home(c, v);
                    break;
                }
            }
        }
        args[i] = lir_vreg(v);
    }

    emit(c, (Instr){
        .op = LIR_CALL,
        .call_name = n->name,
        .nargs = n->nargs,
        .call_args = args,
    });

    int dst = fresh(c);
    emit(c, (Instr){
        .op = LIR_MOV, .dst = dst, .a = lir_phys(PHYS_RAX) });
    return dst;
}

static int lower_expr(LowerCtx *c, Node *n)
{
    if (n->var_decay)
        return lower_addr(c, n);

    switch (n->kind) {
    case ND_NUM: {
        int dst = fresh(c);
        emit(c, (Instr){ .op = LIR_MOVI, .dst = dst, .a = lir_imm(n->val) });
        return dst;
    }
    case ND_VAR: {
        int home = lir_home_vreg(c->lf, n->offset);
        if (home != LIR_NO_VREG)
            return home;
        int dst = fresh(c);
        emit_load_slot(c, dst, n->ty, n->offset);
        return dst;
    }
    case ND_CALL:
        return lower_call(c, n);
    case ND_NEG: {
        int v = lower_expr(c, n->operand);
        int dst = fresh(c);
        emit(c, (Instr){
            .op = LIR_NEG, .dst = dst, .a = lir_vreg(v), .w = expr_width(n->operand) });
        return dst;
    }
    case ND_ADDR: {
        int dst = fresh(c);
        switch (n->operand->kind) {
        case ND_VAR:
            emit(c, (Instr){
                .op = LIR_LEA,
                .dst = dst,
                .a = lir_mem(LIR_FP, n->operand->offset),
            });
            return dst;
        case ND_DEREF:
            return lower_expr(c, n->operand->operand);
        case ND_MEMBER:
            return lower_member_addr(c, n->operand);
        default:
            assert(0);
            return dst;
        }
    }
    case ND_DEREF: {
        int addr = lower_addr(c, n);
        int dst = fresh(c);
        int bytes = store_width_bytes(n->ty);
        emit(c, (Instr){
            .op = LIR_LOAD,
            .dst = dst,
            .a = lir_mem(addr, 0),
            .w = load_lir_width(n->ty),
            .sgn = load_sign(n->ty),
            .aux = bytes,
        });
        emit_widen_to_rax(c, dst, n->ty);
        return dst;
    }
    case ND_MEMBER: {
        Member *m = member_meta(n);
        if (m->is_bitfield && m->bit_width > 0)
            return lower_bitfield_load(c, n, m);
        int addr = lower_member_addr(c, n);
        int dst = fresh(c);
        int bytes = store_width_bytes(n->ty);
        emit(c, (Instr){
            .op = LIR_LOAD,
            .dst = dst,
            .a = lir_mem(addr, 0),
            .w = load_lir_width(n->ty),
            .sgn = load_sign(n->ty),
            .aux = bytes,
        });
        emit_widen_to_rax(c, dst, n->ty);
        return dst;
    }
    case ND_CAST: {
        int dst = fresh(c);
        lower_cast_into(c, dst, n);
        return dst;
    }
    case ND_ASSIGN: {
        if (type_is_record(n->lhs->ty)) {
            int dst = lower_object_addr(c, n->lhs);

            if (is_struct_scalar_cast(n->rhs)) {
                int val = fresh(c);
                lower_struct_from_scalar(c, dst, n->lhs->ty, n->rhs->operand);
                emit(c, (Instr){ .op = LIR_MOVI, .dst = val, .a = lir_imm(0) });
                return val;
            }
            {
                int src = lower_object_addr(c, n->rhs);
                int val = fresh(c);

                emit_memcpy(c, dst, src, type_size(n->lhs->ty));
                emit(c, (Instr){ .op = LIR_MOVI, .dst = val, .a = lir_imm(0) });
                return val;
            }
        }
        int val = lower_expr(c, n->rhs);
        if (n->lhs->kind == ND_VAR) {
            int home = lir_home_vreg(c->lf, n->lhs->offset);
            if (home != LIR_NO_VREG) {
                if (val != home)
                    emit(c, (Instr){
                        .op = LIR_MOV, .dst = home, .a = lir_vreg(val) });
                return val;
            }
            emit_store_slot(c, val, n->lhs->ty, n->lhs->offset);
            return val;
        }
        if (n->lhs->kind == ND_MEMBER) {
            Member *m = member_meta(n->lhs);
            if (m->is_bitfield && m->bit_width > 0) {
                lower_bitfield_store(c, n->lhs, m, val);
                return val;
            }
            int addr = lower_member_addr(c, n->lhs);
            emit(c, (Instr){
                .op = LIR_STORE,
                .a = lir_mem(addr, 0),
                .b = lir_vreg(val),
                .w = store_lir_width(n->lhs->ty),
                .aux = store_width_bytes(n->lhs->ty),
            });
            return val;
        }
        int addr = lower_addr(c, n->lhs);
        emit(c, (Instr){
            .op = LIR_STORE,
            .a = lir_mem(addr, 0),
            .b = lir_vreg(val),
            .w = store_lir_width(n->lhs->ty),
            .aux = store_width_bytes(n->lhs->ty),
        });
        return val;
    }
    case ND_BINOP: {
        int dst = fresh(c);
        lower_binop(c, dst, n->op, n->lhs, n->rhs, n->ty);
        return dst;
    }
    default:
        assert(0);
        return fresh(c);
    }
}

static void lower_cond_branch(LowerCtx *c, Node *cond, int false_label)
{
    if (cond->kind == ND_BINOP && is_cmp(cond->op)) {
        int w = binop_width(cond->lhs, cond->rhs);
        int vl = lower_expr(c, cond->lhs);
        vl = protect_home_before(c, vl, cond->rhs);
        Operand rb;
        if (cond->rhs->kind == ND_NUM && fits_imm32(cond->rhs->val))
            rb = lir_imm(cond->rhs->val);
        else
            rb = lir_vreg(lower_expr(c, cond->rhs));
        emit(c, (Instr){
            .op = LIR_BR,
            .a = lir_vreg(vl),
            .b = rb,
            .w = (LirWidth)w,
            .cc = false_branch_cc(cond->op),
            .sgn = binop_sign(cond->lhs, cond->rhs, cond->ty, cond->op),
            .label = false_label,
        });
        return;
    }

    int v = lower_expr(c, cond);
    emit(c, (Instr){
        .op = LIR_BR,
        .a = lir_vreg(v),
        .b = lir_imm(0),
        .w = expr_width(cond),
        .cc = CC_EQ,
        .label = false_label,
    });
}

static void lower_stmt(LowerCtx *c, Node *n)
{
    switch (n->kind) {
    case ND_RETURN: {
        if (n->operand) {
            int v = lower_expr(c, n->operand);
            emit(c, (Instr){
                .op = LIR_MOV,
                .dst = LIR_NO_VREG,
                .a = lir_vreg(v),
                .b = lir_phys(PHYS_RAX),
            });
        }
        emit(c, (Instr){ .op = LIR_JMP, .label = c->ret_label });
        return;
    }
    case ND_EXPR_STMT:
        (void)lower_expr(c, n->operand);
        return;
    case ND_DECL:
        if (can_promote(c->fn, n->ty, n->offset)) {
            int v = fresh(c);
            lir_bind_home(c->lf, n->offset, v);
            if (n->init) {
                int val = lower_expr(c, n->init);
                if (val != v)
                    emit(c, (Instr){
                        .op = LIR_MOV, .dst = v, .a = lir_vreg(val) });
            }
            return;
        }
        if (n->init && n->init->kind == ND_INIT_LIST) {
            lower_brace_init(c, n);
            return;
        }
        if (n->init && is_struct_scalar_cast(n->init)) {
            int addr = fresh(c);
            emit(c, (Instr){
                .op = LIR_LEA,
                .dst = addr,
                .a = lir_mem(LIR_FP, n->offset),
            });
            lower_struct_from_scalar(c, addr, n->ty, n->init->operand);
            return;
        }
        if (n->init) {
            int v = lower_expr(c, n->init);
            emit_store_slot(c, v, n->ty, n->offset);
        }
        return;
    case ND_IF: {
        int id = lir_new_label(c->lf);
        int end_id = lir_new_label(c->lf);
        lower_cond_branch(c, n->cond, n->else_body ? id : end_id);
        lower_stmt(c, n->then_body);
        if (n->else_body) {
            emit(c, (Instr){ .op = LIR_JMP, .label = end_id });
            emit(c, (Instr){ .op = LIR_LABEL, .label = id });
            lower_stmt(c, n->else_body);
            emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        } else {
            emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        }
        return;
    }
    case ND_WHILE: {
        int begin = emit(c, (Instr){ .op = LIR_LABEL, .label = lir_new_label(c->lf) });
        int end_id = lir_new_label(c->lf);
        lower_cond_branch(c, n->cond, end_id);
        lower_stmt(c, n->then_body);
        emit(c, (Instr){ .op = LIR_JMP, .label = c->lf->instrs[begin].label });
        int end_idx = emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        lir_add_loop(c->lf, begin, end_idx);
        return;
    }
    case ND_FOR: {
        if (n->init)
            lower_expr(c, n->init);
        int begin = emit(c, (Instr){ .op = LIR_LABEL, .label = lir_new_label(c->lf) });
        int end_id = lir_new_label(c->lf);
        if (n->cond)
            lower_cond_branch(c, n->cond, end_id);
        lower_stmt(c, n->then_body);
        if (n->step)
            lower_expr(c, n->step);
        emit(c, (Instr){ .op = LIR_JMP, .label = c->lf->instrs[begin].label });
        int end_idx = emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        lir_add_loop(c->lf, begin, end_idx);
        return;
    }
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next)
            lower_stmt(c, s);
        return;
    default:
        return;
    }
}

static int stmt_returns(Node *n);

static int stmt_list_returns(Node *body)
{
    for (Node *s = body; s; s = s->next)
        if (stmt_returns(s))
            return 1;
    return 0;
}

static int stmt_returns(Node *n)
{
    switch (n->kind) {
    case ND_RETURN:
        return 1;
    case ND_IF:
        return n->else_body &&
               stmt_returns(n->then_body) &&
               stmt_returns(n->else_body);
    case ND_BLOCK:
        return stmt_list_returns(n->body);
    default:
        return 0;
    }
}

LirFn *lower_function(Function *fn)
{
    LirFn *lf = lir_fn_new(fn->name);
    LowerCtx ctx = { .lf = lf, .fn = fn, .ret_label = lir_new_label(lf) };
    lf->epilogue_label = ctx.ret_label;

    lower_params(&ctx);

    for (Node *s = fn->body; s; s = s->next)
        lower_stmt(&ctx, s);

    if (!stmt_list_returns(fn->body)) {
        emit(&ctx, (Instr){
            .op = LIR_MOV,
            .dst = LIR_NO_VREG,
            .a = lir_imm(0),
            .b = lir_phys(PHYS_RAX),
        });
    }

    emit(&ctx, (Instr){ .op = LIR_LABEL, .label = ctx.ret_label });
    emit(&ctx, (Instr){ .op = LIR_RET, .a = lir_phys(PHYS_RAX) });

    return lf;
}
