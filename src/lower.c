/* SPDX-License-Identifier: MIT */
#include "lower.h"
#include "target.h"
#include "abi_sysv_amd64.h"
#include "diag.h"
#include "arena.h"
#include "sema_scope.h"
#include "type.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

typedef struct ControlCtx ControlCtx;

struct ControlCtx {
    int continue_label;
    int break_label;
    ControlCtx *prev;
};

typedef struct {
    LirFn *lf;
    Function *fn;
    int ret_label;
    LirBlockId current;
    ControlCtx *control;
} LowerCtx;

static LirBlock *current_block(LowerCtx *c)
{
    return lir_get_block(c->lf, c->current);
}

static void start_unreachable_block(LowerCtx *c)
{
    if (current_block(c)->term.kind != LIR_TERM_NONE)
        c->current = lir_new_block(c->lf);
}

static int emit(LowerCtx *c, Instr ins)
{
    LirBlock *block;

    if (ins.op == LIR_LABEL) {
        LirBlockId target = lir_label_block(c->lf, ins.label);
        block = current_block(c);
        if (block->term.kind == LIR_TERM_NONE && block->id != target) {
            block->term.kind = LIR_TERM_JMP;
            block->term.target = target;
        }
        c->current = target;
        return 0;
    }

    start_unreachable_block(c);
    block = current_block(c);
    if (ins.op == LIR_JMP) {
        block->term.kind = LIR_TERM_JMP;
        block->term.target = lir_label_block(c->lf, ins.label);
        return 0;
    }
    if (ins.op == LIR_BR) {
        LirBlockId fallthrough = lir_new_block(c->lf);
        block = current_block(c);
        block->term.kind = LIR_TERM_BR;
        block->term.true_target = lir_label_block(c->lf, ins.label);
        block->term.false_target = fallthrough;
        block->term.a = ins.a;
        block->term.b = ins.b;
        block->term.w = ins.w;
        block->term.sgn = ins.sgn;
        block->term.fpw = ins.fpw;
        block->term.cc = ins.cc;
        c->current = fallthrough;
        return 0;
    }
    if (ins.op == LIR_RET) {
        block->term.kind = LIR_TERM_RET;
        block->term.a = ins.a;
        return 0;
    }
    return lir_block_emit(block, ins);
}

static int fresh(LowerCtx *c)
{
    return lir_new_vreg(c->lf);
}

static LirType floating_lir_type(Type *ty)
{
    switch (type_unqualified(ty)->float_width) {
    case FW_FLOAT:       return LIR_TYPE_F32;
    case FW_DOUBLE:      return LIR_TYPE_F64;
    case FW_LONG_DOUBLE: return LIR_TYPE_F80;
    }
    diag_fatal("internal error: invalid floating type");
    return LIR_TYPE_F64;
}

static int fresh_type(LowerCtx *c, Type *ty)
{
    if (!type_is_floating(ty))
        return lir_new_vreg_type(c->lf, LIR_TYPE_I64);
    return lir_new_vreg_type(c->lf, floating_lir_type(ty));
}

static int protect_home_before(LowerCtx *c, int v, Node *later)
{
    (void)c;
    (void)later;
    return v;
}

static int emit_load_slot(LowerCtx *c, int dst, Type *ty, int offset);
static void emit_store_slot(LowerCtx *c, int src, Type *ty, int offset);
static void lower_bitfield_store_off(LowerCtx *c, int struct_off, Member *m, int val);
static int lower_cast_value(LowerCtx *c, Node *n);
static int convert_value(LowerCtx *c, int v, Type *from, Type *to);

static FrameLocal *param_frame_local(LowerCtx *c, int offset)
{
    for (int i = 0; i < c->fn->nframe_locals; i++) {
        FrameLocal *local = &c->fn->frame_locals[i];
        if (local->offset == offset)
            return local;
    }
    return NULL;
}

static void lower_params(LowerCtx *c)
{
    int nparams = c->fn->nparams;
    Param **params = arena_alloc((size_t)nparams * sizeof(*params));
    int *raws = arena_alloc((size_t)nparams * sizeof(*raws));
    int at = 0;

    for (Param *p = c->fn->params; p; p = p->next) {
        params[at] = p;
        raws[at] = LIR_NO_VREG;
        at++;
    }

    /* Capture every incoming register before normalizing any one parameter:
       an allocated conversion result must not overwrite a later ABI input. */
    for (int i = 0; i < nparams; i++) {
        Param *p = params[i];
        TypeScalarInfo si;
        FrameLocal *local = param_frame_local(c, p->offset);
        int raw;

        if (p->abi_gpr_start < 0 || p->abi_ngpr != 1 || !local ||
            !local->promotable_scalar || local->address_taken ||
            !type_scalar_info(type_decay(p->ty), &si))
            continue;

        raw = fresh(c);
        emit(c, (Instr){
            .op = LIR_MOV,
            .dst = raw,
            .a = lir_phys(X86_SYSV.arg_regs[p->abi_gpr_start]),
        });
        lir_precolor_vreg(c->lf, raw,
                          X86_SYSV.arg_regs[p->abi_gpr_start]);
        raws[i] = raw;
    }

    for (int i = 0; i < nparams; i++) {
        Param *p = params[i];
        TypeScalarInfo si;
        int raw = raws[i];
        int value;

        if (raw == LIR_NO_VREG)
            continue;
        (void)type_scalar_info(type_decay(p->ty), &si);
        if (si.width == 4) {
            value = fresh(c);
            emit(c, (Instr){
                .op = LIR_CONV,
                .dst = value,
                .a = lir_vreg(raw),
                .conv = si.is_signed ? CONV_TRUNC_LO32 : CONV_ZEXT32,
            });
        } else {
            value = fresh(c);
            emit(c, (Instr){
                .op = LIR_MOV,
                .dst = value,
                .a = lir_vreg(raw),
            });
        }
        lir_bind_home(c->lf, p->offset, value);
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

    if (type_is_floating(ty))
        return type_size(ty);
    if (type_scalar_info(ty, &si))
        return si.width;
    return 0;
}

static LirWidth store_lir_width(Type *ty)
{
    int w = store_width_bytes(ty);
    return w == 8 ? LIR_W8 : LIR_W4;
}

static LirFloatWidth float_width(Type *ty)
{
    switch (type_unqualified(ty)->float_width) {
    case FW_FLOAT:       return LIR_FP_F32;
    case FW_DOUBLE:      return LIR_FP_F64;
    case FW_LONG_DOUBLE: return LIR_FP_F80;
    }
    diag_fatal("internal error: invalid floating type");
    return LIR_FP_NONE;
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

static int widen_loaded_value(LowerCtx *c, int v, Type *ty)
{
    if (load_lir_width(ty) == LIR_W8)
        return v;
    if (type_is_char(ty))
        return v;
    if (load_lir_width(ty) == LIR_W4 && !type_is_short(ty))
        return v;
    if (type_is_short(ty) && load_sign(ty) == LIR_SGN_S) {
        int t = fresh(c);
        emit(c, (Instr){
            .op = LIR_CONV, .dst = t, .a = lir_vreg(v), .conv = CONV_SEXT16 });
        v = t;
    }
    int dst = fresh(c);
    emit(c, (Instr){
        .op = LIR_CONV, .dst = dst, .a = lir_vreg(v), .conv = CONV_SEXT32_64 });
    return dst;
}

static int emit_load_slot(LowerCtx *c, int dst, Type *ty, int offset)
{
    int bytes = store_width_bytes(ty);
    emit(c, (Instr){
        .op = LIR_LOAD,
        .dst = dst,
        .a = lir_mem(LIR_FP, offset),
        .w = load_lir_width(ty),
        .sgn = load_sign(ty),
        .fpw = type_is_floating(ty) ? float_width(ty) : LIR_FP_NONE,
        .aux = bytes,
    });
    return widen_loaded_value(c, dst, ty);
}

static void emit_store_slot(LowerCtx *c, int src, Type *ty, int offset)
{
    emit(c, (Instr){
        .op = LIR_STORE,
        .a = lir_mem(LIR_FP, offset),
        .b = lir_vreg(src),
        .w = store_lir_width(ty),
        .fpw = type_is_floating(ty) ? float_width(ty) : LIR_FP_NONE,
        .aux = store_width_bytes(ty),
    });
}

#if 0 /* In-place `addl $1, off(%rbp)` for ++/-- on stack locals.
       * Disabled: lowering this before mem2reg updates memory while the
       * promoted SSA vreg (e.g. loop index i) is unchanged, causing infinite
       * loops. Re-enable only with a late pass that knows the slot is not
       * register-resident (after mem2reg/regalloc), not from lower. */
static void emit_binop_imm_fp_slot(LowerCtx *c, int offset, Type *ty,
                                    LirOp op, long imm)
{
    emit(c, (Instr){
        .op = op,
        .dst = LIR_NO_VREG,
        .a = lir_mem(LIR_FP, offset),
        .b = lir_imm(imm),
        .w = store_lir_width(ty),
        .aux = store_width_bytes(ty),
    });
}

static int is_local_integer_lvalue(Node *lv)
{
    return lv->kind == ND_VAR &&
           lv->storage == VAR_STORAGE_LOCAL &&
           type_is_integer(lv->ty);
}
#endif

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

static int lower_symbol_addr(LowerCtx *c, const char *name)
{
    int dst = fresh(c);
    emit(c, (Instr){
        .op = LIR_LEA_SYM,
        .dst = dst,
        .sym_name = (char *)name,
    });
    return dst;
}

static char *node_symbol_name(const Node *n)
{
    return n->symbol_name ? n->symbol_name : n->name;
}

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

static int imm_pow2_log2(long n)
{
    int k;

    if (n <= 0)
        return -1;
    k = 0;
    while ((n & 1) == 0) {
        n >>= 1;
        k++;
    }
    return n == 1 ? k : -1;
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
static void lower_branch(LowerCtx *c, Node *n, int true_label,
                         int false_label);
static int lower_member_addr(LowerCtx *c, Node *n);
static int lower_call_ex(LowerCtx *c, Node *n, int result_off);
static Member *member_meta(Node *n);
static void lower_bitfield_store(LowerCtx *c, Node *lhs, Member *m, int val);

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
        Type *record = type_unqualified(ty);

        for (i = 0; i < record->nmembers; i++) {
            Member *m = &record->members[i];

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
        Type *record = type_unqualified(ty);

        if (record->nmembers > 0)
            lower_init_from_type(c, record->members[0].ty, pcursor, base_off);
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
        if (n->storage == VAR_STORAGE_GLOBAL)
            return lower_symbol_addr(c, node_symbol_name(n));
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
    case ND_CALL: {
        int tmp = c->fn->abi_call_scratch;
        int dst;

        if (!tmp)
            diag_fatal("internal error: record call result needs scratch slot");
        (void)lower_call_ex(c, n, tmp);
        dst = fresh(c);
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem(LIR_FP, tmp),
        });
        return dst;
    }
    default:
        assert(0 && "invalid object address");
        return fresh(c);
    }
}

static void control_push(LowerCtx *c, ControlCtx *control,
                         int continue_label, int break_label)
{
    control->continue_label = continue_label;
    control->break_label = break_label;
    control->prev = c->control;
    c->control = control;
}

static void control_pop(LowerCtx *c)
{
    c->control = c->control->prev;
}

static void lower_stmt(LowerCtx *c, Node *n);

static int named_label_id(LowerCtx *c, Node *label)
{
    if (label->label < 0)
        label->label = lir_new_label(c->lf);
    return label->label;
}

static Member *member_meta(Node *n)
{
    Type *record = type_unqualified(n->lhs->ty);

    return &record->members[n->member_index];
}

static int var_decl_is_enum(const char *name, Function *fn)
{
    Type *ty;

    if (scope_lookup(name, NULL, &ty) && type_is_enum(ty))
        return 1;
    for (Param *p = fn->params; p; p = p->next) {
        if (p->name && strcmp(p->name, name) == 0 && type_is_enum(p->ty))
            return 1;
    }
    return 0;
}

static int expr_enum_domain(Node *n, Function *fn)
{
    if (!n)
        return 0;

    switch (n->kind) {
    case ND_VAR:
        return var_decl_is_enum(n->name, fn);
    case ND_NUM:
        return 1;
    case ND_MEMBER: {
        Member *m = member_meta(n);
        return m && type_is_enum(m->ty);
    }
    case ND_BINOP:
        if (n->op == OP_ADD || n->op == OP_SUB)
            return expr_enum_domain(n->lhs, fn) &&
                   expr_enum_domain(n->rhs, fn);
        return 0;
    case ND_CAST:
        return expr_enum_domain(n->operand, fn);
    default:
        return 0;
    }
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
    int conv = fresh(c);
    emit(c, (Instr){
        .op = LIR_CONV, .dst = conv, .a = lir_vreg(dst), .conv = CONV_ZEXT32 });
    return conv;
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
    if (base->kind == ND_VAR && !base->var_decay &&
        base->storage == VAR_STORAGE_LOCAL)
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
        int conv = fresh(c);
        emit(c, (Instr){
            .op = LIR_CONV, .dst = conv, .a = lir_vreg(dst), .conv = CONV_ZEXT32 });
        return conv;
    }
}

static void lower_bitfield_unit_store(LowerCtx *c, Node *base, int unit_off, int val)
{
    if (base->kind == ND_VAR && !base->var_decay &&
        base->storage == VAR_STORAGE_LOCAL) {
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
        int s1 = fresh(c);
        int s2 = fresh(c);
        emit_binop_imm(c, LIR_SHL, s1, dst, sh);
        emit_binop_imm(c, LIR_SAR, s2, s1, sh);
        return s2;
    }
    return dst;
}

static void lower_bitfield_store(LowerCtx *c, Node *lhs, Member *m, int val)
{
    if (lhs->lhs->kind == ND_VAR && !lhs->lhs->var_decay &&
        lhs->lhs->storage == VAR_STORAGE_LOCAL) {
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
    Type *sty = type_unqualified(n->lhs->ty);
    Member *m = &sty->members[n->member_index];
    int dst = fresh(c);

    if (n->lhs->kind == ND_CALL && abi_type_is_record_pass(n->lhs->ty)) {
        int tmp = c->fn->abi_call_scratch;

        if (!tmp)
            diag_fatal("internal error: struct call result needs scratch slot");
        (void)lower_call_ex(c, n->lhs, tmp);
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem(LIR_FP, tmp + m->offset),
        });
        return dst;
    }

    if (n->lhs->kind == ND_VAR && !n->lhs->var_decay &&
        n->lhs->storage == VAR_STORAGE_LOCAL) {
        emit(c, (Instr){
            .op = LIR_LEA,
            .dst = dst,
            .a = lir_mem(LIR_FP, n->lhs->offset + m->offset),
        });
        return dst;
    }

    {
        int base = lower_object_addr(c, n->lhs);
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
    case ND_STRING:
        return lower_symbol_addr(c, n->string_label);
    case ND_VAR:
        if (n->storage == VAR_STORAGE_GLOBAL) {
            int addr = lower_symbol_addr(c, node_symbol_name(n));
            if (n->var_decay)
                return addr;
            int dst = fresh(c);
            emit(c, (Instr){
                .op = LIR_LOAD,
                .dst = dst,
                .a = lir_mem(addr, 0),
                .w = load_lir_width(n->ty),
                .sgn = load_sign(n->ty),
                .aux = store_width_bytes(n->ty),
            });
            return widen_loaded_value(c, dst, n->ty);
        }
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
            return emit_load_slot(c, dst, n->ty, n->offset);
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

static int emit_conv_value(LowerCtx *c, int src, ConvKind k)
{
    int to_f32 = k == CONV_SI32_F32 || k == CONV_SI64_F32 ||
                 k == CONV_UI32_F32 || k == CONV_UI64_F32 ||
                 k == CONV_F64_F32 || k == CONV_F80_F32;
    int to_f64 = k == CONV_SI32_F64 || k == CONV_SI64_F64 ||
                 k == CONV_UI32_F64 || k == CONV_UI64_F64 ||
                 k == CONV_F32_F64 || k == CONV_F80_F64;
    int to_f80 = k == CONV_F32_F80 || k == CONV_F64_F80 ||
                 k == CONV_SI32_F80 || k == CONV_SI64_F80 ||
                 k == CONV_UI32_F80 || k == CONV_UI64_F80;
    LirFloatWidth fpw = (k == CONV_SI32_F32 || k == CONV_SI64_F32 ||
                         k == CONV_UI32_F32 || k == CONV_F64_F32 ||
                         k == CONV_UI64_F32 ||
                         k == CONV_F32_SI32 || k == CONV_F32_SI64 ||
                         k == CONV_F32_UI32 || k == CONV_F32_UI64)
                        ? LIR_FP_F32 : to_f80 ||
                          k == CONV_F80_F32 || k == CONV_F80_F64 ||
                          k == CONV_F80_SI32 || k == CONV_F80_SI64 ||
                          k == CONV_F80_UI32 || k == CONV_F80_UI64
                        ? LIR_FP_F80 : LIR_FP_F64;
    int dst = lir_new_vreg_type(c->lf,
        to_f80 ? LIR_TYPE_F80 :
        to_f32 ? LIR_TYPE_F32 : to_f64 ? LIR_TYPE_F64 :
        LIR_TYPE_I64);
    emit(c, (Instr){ .op = LIR_CONV, .dst = dst, .a = lir_vreg(src),
                     .conv = k, .fpw = fpw });
    return dst;
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
    int t;

    if (sz > store_bytes)
        lower_mem_zero(c, addr, sz);

    t = convert_value(c, val, scalar->ty, type_long());
    emit(c, (Instr){
        .op = LIR_STORE,
        .a = lir_mem(addr, 0),
        .b = lir_vreg(t),
        .w = store_lir_width(scalar->ty),
        .aux = store_bytes,
    });
}

static int convert_value(LowerCtx *c, int v, Type *from, Type *to)
{
    if (type_is_void(to))
        return v;

    if (type_is_floating(from) || type_is_floating(to)) {
        if (type_same(type_unqualified(from), type_unqualified(to)))
            return v;
        if (type_is_floating(from) && type_is_floating(to)) {
            LirFloatWidth src = float_width(from);
            LirFloatWidth dst = float_width(to);

            if (dst == LIR_FP_F80)
                return emit_conv_value(c, v, src == LIR_FP_F32
                    ? CONV_F32_F80 : CONV_F64_F80);
            if (src == LIR_FP_F80)
                return emit_conv_value(c, v, dst == LIR_FP_F32
                    ? CONV_F80_F32 : CONV_F80_F64);
            return emit_conv_value(c, v, dst == LIR_FP_F64
                ? CONV_F32_F64 : CONV_F64_F32);
        }
        if (type_is_integer(from) && type_is_floating(to)) {
            int wide = type_int_width(from) == 8;
            LirFloatWidth dst = float_width(to);

            if (dst == LIR_FP_F80) {
                if (!type_is_signed(from))
                    return emit_conv_value(c, v, wide
                        ? CONV_UI64_F80 : CONV_UI32_F80);
                return emit_conv_value(c, v, wide
                    ? CONV_SI64_F80 : CONV_SI32_F80);
            }
            int f64 = dst == LIR_FP_F64;
            if (!type_is_signed(from) && wide)
                return emit_conv_value(c, v, f64 ? CONV_UI64_F64 : CONV_UI64_F32);
            if (!type_is_signed(from) && !wide)
                return emit_conv_value(c, v, f64 ? CONV_UI32_F64 : CONV_UI32_F32);
            return emit_conv_value(c, v,
                wide ? (f64 ? CONV_SI64_F64 : CONV_SI64_F32)
                     : (f64 ? CONV_SI32_F64 : CONV_SI32_F32));
        }
        if (type_is_floating(from) && type_is_integer(to)) {
            int wide = type_int_width(to) == 8;
            LirFloatWidth src = float_width(from);

            if (src == LIR_FP_F80) {
                if (!type_is_signed(to))
                    return emit_conv_value(c, v, wide
                        ? CONV_F80_UI64 : CONV_F80_UI32);
                return emit_conv_value(c, v, wide
                    ? CONV_F80_SI64 : CONV_F80_SI32);
            }
            if (!type_is_signed(to) && wide)
                return emit_conv_value(c, v, src == LIR_FP_F32
                    ? CONV_F32_UI64 : CONV_F64_UI64);
            if (!type_is_signed(to) && !wide)
                return emit_conv_value(c, v, src == LIR_FP_F32
                    ? CONV_F32_UI32 : CONV_F64_UI32);
            return emit_conv_value(c, v,
                src == LIR_FP_F32
                    ? (wide ? CONV_F32_SI64 : CONV_F32_SI32)
                    : (wide ? CONV_F64_SI64 : CONV_F64_SI32));
        }
        return v;
    }

    TypeScalarInfo di;
    TypeScalarInfo si;
    if (!type_scalar_info(to, &di) || !type_scalar_info(from, &si))
        return v;

    int dw = di.width;
    int ds = di.is_signed;
    int sw = si.width;
    int ss = si.is_signed;

    /* No representation change. */
    if (dw == sw && ds == ss)
        return v;

    if (dw >= 8) {
        /* Widen to 64 bits preserving the source value (per source sign). */
        if (sw >= 8)
            return v;
        if (sw == 4) {
            v = emit_conv_value(c, v, ss ? CONV_SEXT32_64 : CONV_ZEXT32);
        } else if (sw == 2) {
            v = emit_conv_value(c, v, ss ? CONV_SEXT16 : CONV_ZEXT16);
        } else {
            if (ss) {
                v = emit_conv_value(c, v, CONV_SEXT8);
                v = emit_conv_value(c, v, CONV_SEXT32_64);
            } else {
                v = emit_conv_value(c, v, CONV_ZEXT8);
            }
        }
        return v;
    }

    if (dw == 4) {
        /* Truncate to 32 bits then re-canonicalise per destination sign.
           A signed int from a <=32-bit source is already canonical. */
        if (sw >= 8)
            v = emit_conv_value(c, v, ds ? CONV_SEXT32_64 : CONV_ZEXT32);
        else if (ds && sw == 4)
            v = emit_conv_value(c, v, CONV_SEXT32_64);
        else if (!ds)
            v = emit_conv_value(c, v, CONV_ZEXT32);
        return v;
    }

    if (dw == 2) {
        return emit_conv_value(c, v, ds ? CONV_SEXT16 : CONV_ZEXT16);
    }

    /* dw == 1 */
    return emit_conv_value(c, v, ds ? CONV_SEXT8 : CONV_ZEXT8);
}

static int lower_cast_value(LowerCtx *c, Node *n)
{
    int v = lower_expr(c, n->operand);
    return convert_value(c, v, n->operand->ty, n->ty);
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
    if (op == OP_COMMA) {
        int vr;

        (void)lower_expr(c, lhs);
        vr = lower_expr(c, rhs);
        emit(c, (Instr){
            .op = LIR_MOV, .dst = dst, .a = lir_vreg(vr) });
        return;
    }

    if (type_is_pointer(lhs->ty) && type_is_pointer(rhs->ty) && op == OP_SUB) {
        lower_ptr_diff(c, dst, lhs, rhs);
        return;
    }

    if (is_ptr_int_arith(op, lhs, rhs)) {
        lower_ptr_int_arith(c, dst, op, lhs, rhs);
        return;
    }


    if (type_is_floating(lhs->ty)) {
        int vl = lower_expr(c, lhs);
        int vr = lower_expr(c, rhs);
        LirFloatWidth fpw = float_width(lhs->ty);

        if (is_cmp(op)) {
            emit(c, (Instr){
                .op = LIR_FSETCC, .dst = dst,
                .a = lir_vreg(vl), .b = lir_vreg(vr),
                .fpw = fpw, .cc = binop_cc(op),
            });
        } else {
            LirOp fop = op == OP_ADD ? LIR_FADD :
                         op == OP_SUB ? LIR_FSUB :
                         op == OP_MUL ? LIR_FMUL : LIR_FDIV;
            emit(c, (Instr){
                .op = fop, .dst = dst,
                .a = lir_vreg(vl), .b = lir_vreg(vr), .fpw = fpw,
            });
        }
        return;
    }

    int w = binop_width(lhs, rhs);
    int vl = lower_expr(c, lhs);
    vl = protect_home_before(c, vl, rhs);
    LirSign sgn = binop_sign(lhs, rhs, res_ty, op);

    /* x86 can materialize a constant divisor in the emitter when needed. */
    int imm_ok = rhs->kind == ND_NUM && fits_imm32(rhs->val);
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
    case OP_BITAND:
    case OP_BITXOR:
    case OP_BITOR:
        emit(c, (Instr){
            .op = op == OP_BITAND ? LIR_AND :
                  op == OP_BITXOR ? LIR_XOR : LIR_OR,
            .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w });
        return;
    case OP_SHL:
    case OP_SHR:
        emit(c, (Instr){
            .op = op == OP_SHL ? LIR_SHL :
                  (sgn == LIR_SGN_U ? LIR_SHR : LIR_SAR),
            .dst = dst, .a = lir_vreg(vl), .b = rb,
            .w = (LirWidth)w, .sgn = sgn });
        return;
    case OP_DIV:
    case OP_MOD:
        if (rhs->kind == ND_NUM && fits_imm32(rhs->val)) {
            int k = imm_pow2_log2(rhs->val);

            if (k >= 0) {
                LirOp pop;

                if (op == OP_MOD) {
                    if (sgn == LIR_SGN_U || expr_enum_domain(lhs, c->fn))
                        pop = LIR_UMOD_POW2;
                    else
                        pop = LIR_SMOD_POW2;
                } else {
                    if (sgn == LIR_SGN_U || expr_enum_domain(lhs, c->fn))
                        pop = LIR_UDIV_POW2;
                    else
                        pop = LIR_SDIV_POW2;
                }
                emit(c, (Instr){
                    .op = pop,
                    .dst = dst,
                    .a = lir_vreg(vl),
                    .aux = k,
                    .w = (LirWidth)w,
                    .sgn = sgn,
                });
                return;
            }
        }
        emit(c, (Instr){
            .op = (op == OP_DIV) ? LIR_DIV : LIR_MOD,
            .dst = dst,
            .a = lir_vreg(vl),
            .b = rb,
            .w = (LirWidth)w,
            .sgn = sgn,
        });
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
        assert(0);
        return;
    }
}

#define CALL_DEST_NONE INT_MIN
#define CALL_SRET_FORWARD (INT_MIN + 1)

static int lower_addr_off(LowerCtx *c, int offset)
{
    int v = fresh(c);
    emit(c, (Instr){
        .op = LIR_LEA,
        .dst = v,
        .a = lir_mem(LIR_FP, offset),
    });
    return v;
}

static void lower_load_bytes_off(LowerCtx *c, int dst, int addr, int off, int bytes)
{
    emit(c, (Instr){
        .op = LIR_LOAD,
        .dst = dst,
        .a = lir_mem(addr, off),
        .w = bytes >= 8 ? LIR_W8 : LIR_W4,
        .sgn = LIR_SGN_Z,
        .aux = bytes >= 8 ? 8 : bytes,
    });
}

static void lower_store_call_result_to_off(LowerCtx *c, int off, const AbiRetPlan *rp)
{
    int v = fresh(c);
    int bytes = rp->size < 8 ? rp->size : 8;

    emit(c, (Instr){
        .op = LIR_MOV,
        .dst = v,
        .a = lir_phys(PHYS_RAX),
    });
    emit(c, (Instr){
        .op = LIR_STORE,
        .a = lir_mem(LIR_FP, off),
        .b = lir_vreg(v),
        .w = bytes >= 8 ? LIR_W8 : LIR_W4,
        .aux = bytes,
    });
    if (rp->kind != ABI_RET_GPR_PAIR)
        return;

    {
        int tail = rp->size - 8;
        int v2 = fresh(c);

        emit(c, (Instr){
            .op = LIR_MOV,
            .dst = v2,
            .a = lir_phys(PHYS_RDX),
        });
        emit(c, (Instr){
            .op = LIR_STORE,
            .a = lir_mem(LIR_FP, off + 8),
            .b = lir_vreg(v2),
            .w = LIR_W8,
            .aux = tail < 8 ? tail : 8,
        });
    }
}

static void append_operand(Operand **items, int *nitems, int *cap, Operand item)
{
    if (*nitems >= *cap) {
        int new_cap;
        Operand *new_items;

        if (*cap > INT_MAX / 2)
            diag_fatal("too many lowered call arguments");
        new_cap = *cap ? *cap * 2 : 16;
        new_items = arena_alloc((size_t)new_cap * sizeof(*new_items));
        if (*items)
            memcpy(new_items, *items, (size_t)*nitems * sizeof(*new_items));
        *items = new_items;
        *cap = new_cap;
    }
    (*items)[(*nitems)++] = item;
}

static int lower_marshal_record_arg(LowerCtx *c, Node *arg, Operand *reg_slots,
                                    int *nreg, Operand **stack_slots,
                                    int *nstack, int *stack_cap)
{
    AbiArgPlan ap;
    Type *ty = arg->ty;
    int addr = lower_object_addr(c, arg);

    abi_arg_plan(ty, &ap);
    if (!abi_arg_fits_gprs(&ap, *nreg, 6)) {
        int nslots = abi_stack_arg_bytes(ap.size) / 8;
        int i;

        for (i = 0; i < nslots; i++) {
            int v = fresh(c);
            int chunk = (i == nslots - 1) ?
                        (ap.size - i * 8) : 8;

            lower_load_bytes_off(c, v, addr, i * 8, chunk < 8 ? chunk : 8);
            append_operand(stack_slots, nstack, stack_cap, lir_vreg(v));
        }
        return 0;
    }

    {
        int v0 = fresh(c);
        int chunk0 = ap.size < 8 ? ap.size : 8;

        lower_load_bytes_off(c, v0, addr, 0, chunk0);
        reg_slots[(*nreg)++] = lir_vreg(v0);
    }
    if (ap.kind == ABI_ARG_GPR_PAIR) {
        int v1 = fresh(c);
        int chunk1 = ap.size - 8;

        lower_load_bytes_off(c, v1, addr, 8, chunk1 < 8 ? chunk1 : 8);
        reg_slots[(*nreg)++] = lir_vreg(v1);
    }
    return 0;
}

/* For (*fp)(...), use the fnptr value — not the dereferenced function type. */
static int lower_call_callee(LowerCtx *c, Node *n)
{
    if (n->kind == ND_DEREF && n->operand && type_is_function_pointer(n->operand->ty))
        return lower_expr(c, n->operand);
    return lower_expr(c, n);
}

static int lower_call_ex(LowerCtx *c, Node *n, int result_off)
{
    Type *ret_ty = n->func_ty ? n->func_ty->ret : type_int();
    AbiRetPlan ret_plan;
    Operand reg_slots[6];
    Operand sse_slots[8];
    Operand *stack_slots = NULL;
    int nreg = 0;
    int nsse = 0;
    int nstack = 0;
    int stack_cap = 0;
    int total;
    Operand *args;
    LirType *arg_types;
    int i;
    int f80_result = LIR_NO_VREG;
    Node *a;

    if (n->nargs > XCC_MAX_CALL_ARGS)
        diag_fatal("internal error: too many call arguments");

    abi_ret_plan(ret_ty, &ret_plan);

    if (ret_plan.kind == ABI_RET_SRET) {
        int dest = result_off;
        int v;

        if (dest == CALL_DEST_NONE) {
            dest = c->fn->abi_call_scratch;
            if (!dest)
                diag_fatal("internal error: sret call without destination");
        }
        if (dest == CALL_SRET_FORWARD) {
            v = fresh(c);
            emit(c, (Instr){
                .op = LIR_LOAD,
                .dst = v,
                .a = lir_mem(LIR_FP, c->fn->abi_sret_offset),
                .w = LIR_W8,
                .aux = 8,
            });
            reg_slots[nreg++] = lir_vreg(v);
        } else {
            reg_slots[nreg++] = lir_vreg(lower_addr_off(c, dest));
        }
    }

    for (a = n->args; a; a = a->next) {
        if (abi_type_is_record_pass(a->ty)) {
            lower_marshal_record_arg(c, a, reg_slots, &nreg,
                                     &stack_slots, &nstack, &stack_cap);
            continue;
        }

        int v = lower_expr(c, a);

        if (type_is_floating(a->ty) && float_width(a->ty) == LIR_FP_F80)
            append_operand(&stack_slots, &nstack, &stack_cap, lir_vreg(v));
        else if (type_is_floating(a->ty) && nsse < 8)
            sse_slots[nsse++] = lir_vreg(v);
        else if (!type_is_floating(a->ty) && nreg < 6)
            reg_slots[nreg++] = lir_vreg(v);
        else
            append_operand(&stack_slots, &nstack, &stack_cap, lir_vreg(v));
    }

    if (nreg > 6)
        diag_fatal("internal error: too many register arguments at call");
    if (nstack > INT_MAX / 8)
        diag_fatal("call argument area is too large");

    total = nreg + nsse + nstack;
    args = arena_alloc((size_t)total * sizeof(*args));
    arg_types = arena_alloc((size_t)total * sizeof(*arg_types));
    for (i = 0; i < nreg; i++)
        args[i] = reg_slots[i];
    for (i = 0; i < nsse; i++)
        args[nreg + i] = sse_slots[i];
    for (i = 0; i < nstack; i++)
        args[nreg + nsse + i] = stack_slots[i];
    for (i = 0; i < total; i++) {
        if (args[i].kind != OPND_VREG)
            diag_fatal("internal error: call argument is not a virtual register");
        arg_types[i] = lir_vreg_type(c->lf, args[i].u.vreg);
    }

    {
        int call_reg = 0;

        if (!n->call_direct)
            call_reg = lower_call_callee(c, n->callee);
        if (type_is_floating(ret_ty) && float_width(ret_ty) == LIR_FP_F80)
            f80_result = fresh_type(c, ret_ty);
        emit(c, (Instr){
            .op = LIR_CALL,
            .dst = f80_result,
            .call_name = n->call_direct ? n->name : NULL,
            .call_indirect = n->call_direct ? 0 : 1,
            .call_reg = call_reg,
            .nargs = total,
            .call_nreg = nreg + nsse,
            .call_ngpr = nreg,
            .call_nsse = nsse,
            .call_args = args,
            .call_arg_types = arg_types,
            .call_ret_type = f80_result == LIR_NO_VREG
                           ? LIR_TYPE_I64 : LIR_TYPE_F80,
        });
    }

    if (abi_type_is_record_pass(ret_ty)) {
        if (ret_plan.kind == ABI_RET_SRET) {
            int dest = result_off;

            if (dest == CALL_DEST_NONE)
                dest = c->fn->abi_call_scratch;
            return lower_addr_off(c, dest);
        }
        if (result_off != CALL_DEST_NONE) {
            lower_store_call_result_to_off(c, result_off, &ret_plan);
            return fresh(c);
        }
        return fresh(c);
    }

    if (f80_result != LIR_NO_VREG)
        return f80_result;

    int dst = fresh_type(c, ret_ty);
    emit(c, (Instr){
        .op = LIR_MOV, .dst = dst,
        .a = lir_phys(type_is_floating(ret_ty) ? PHYS_XMM0 : PHYS_RAX),
        .fpw = type_is_floating(ret_ty) ? float_width(ret_ty) : LIR_FP_NONE });
    return dst;
}

static int lower_call(LowerCtx *c, Node *n)
{
    return lower_call_ex(c, n, CALL_DEST_NONE);
}

static void lower_return_record(LowerCtx *c, Node *n, Type *ret_ty)
{
    AbiRetPlan rp;
    int addr;

    abi_ret_plan(ret_ty, &rp);

    if (n->kind == ND_CALL) {
        if (rp.kind == ABI_RET_SRET)
            (void)lower_call_ex(c, n, CALL_SRET_FORWARD);
        else
            (void)lower_call_ex(c, n, CALL_DEST_NONE);
        emit(c, (Instr){ .op = LIR_JMP, .label = c->ret_label });
        return;
    }

    if (rp.kind == ABI_RET_SRET) {
        int src = lower_object_addr(c, n);
        int dst = fresh(c);

        emit(c, (Instr){
            .op = LIR_LOAD,
            .dst = dst,
            .a = lir_mem(LIR_FP, c->fn->abi_sret_offset),
            .w = LIR_W8,
            .aux = 8,
        });
        emit(c, (Instr){
            .op = LIR_MEMCPY,
            .a = lir_vreg(dst),
            .b = lir_vreg(src),
            .aux = rp.size,
        });
        emit(c, (Instr){
            .op = LIR_MOV,
            .dst = LIR_NO_VREG,
            .a = lir_vreg(dst),
            .b = lir_phys(PHYS_RAX),
        });
        emit(c, (Instr){ .op = LIR_JMP, .label = c->ret_label });
        return;
    }

    addr = lower_object_addr(c, n);
    if (rp.kind == ABI_RET_GPR_PAIR) {
        int v0 = fresh(c);
        int v1 = fresh(c);

        lower_load_bytes_off(c, v0, addr, 0, 8);
        lower_load_bytes_off(c, v1, addr, 8, rp.size - 8);
        emit(c, (Instr){
            .op = LIR_MOV,
            .dst = LIR_NO_VREG,
            .a = lir_vreg(v0),
            .b = lir_phys(PHYS_RAX),
        });
        emit(c, (Instr){
            .op = LIR_MOV,
            .dst = LIR_NO_VREG,
            .a = lir_vreg(v1),
            .b = lir_phys(PHYS_RDX),
        });
        emit(c, (Instr){ .op = LIR_JMP, .label = c->ret_label });
        return;
    }

    {
        int v0 = fresh(c);

        lower_load_bytes_off(c, v0, addr, 0, rp.size < 8 ? rp.size : 8);
        emit(c, (Instr){
            .op = LIR_MOV,
            .dst = LIR_NO_VREG,
            .a = lir_vreg(v0),
            .b = lir_phys(PHYS_RAX),
        });
    }
    emit(c, (Instr){ .op = LIR_JMP, .label = c->ret_label });
}

typedef struct {
    Node *lhs;
    Member *bitfield;
    int direct_slot;
    int offset;
    int addr;
    int bitfield_unit;
} PreparedLvalue;

static PreparedLvalue prepare_lvalue(LowerCtx *c, Node *lhs)
{
    PreparedLvalue lv = { .lhs = lhs, .addr = LIR_NO_VREG,
                          .bitfield_unit = LIR_NO_VREG };

    if (lhs->kind == ND_VAR && lhs->storage == VAR_STORAGE_LOCAL) {
        lv.direct_slot = 1;
        lv.offset = lhs->offset;
        return lv;
    }
    if (lhs->kind == ND_MEMBER) {
        Member *m = member_meta(lhs);

        if (m->is_bitfield && m->bit_width > 0) {
            int base = lower_object_addr(c, lhs->lhs);
            int addr = fresh(c);

            emit(c, (Instr){
                .op = LIR_LEA, .dst = addr, .a = lir_mem(base, m->offset),
            });
            lv.bitfield = m;
            lv.addr = addr;
            return lv;
        }
        lv.addr = lower_member_addr(c, lhs);
        return lv;
    }
    if (lhs->kind == ND_VAR) {
        lv.addr = lower_symbol_addr(c, node_symbol_name(lhs));
        return lv;
    }
    lv.addr = lower_addr(c, lhs);
    return lv;
}

static int load_prepared_lvalue(LowerCtx *c, PreparedLvalue *lv)
{
    Type *ty = lv->lhs->ty;
    int raw = fresh_type(c, ty);

    if (lv->bitfield) {
        Member *m = lv->bitfield;
        int shifted;
        int value;
        long mask = (1L << m->bit_width) - 1;

        emit(c, (Instr){
            .op = LIR_LOAD, .dst = raw, .a = lir_mem(lv->addr, 0),
            .w = LIR_W4, .sgn = LIR_SGN_Z, .aux = 4,
        });
        lv->bitfield_unit = fresh(c);
        emit(c, (Instr){
            .op = LIR_CONV, .dst = lv->bitfield_unit,
            .a = lir_vreg(raw), .conv = CONV_ZEXT32,
        });
        shifted = fresh(c);
        value = fresh(c);
        emit_binop_imm(c, LIR_SHR, shifted, lv->bitfield_unit, m->bit_offset);
        emit_binop_imm(c, LIR_AND, value, shifted, mask);
        if (type_is_signed(m->ty) && !type_is_unsigned(m->ty)) {
            int sh = 64 - m->bit_width;
            int left = fresh(c);
            int signed_value = fresh(c);
            emit_binop_imm(c, LIR_SHL, left, value, sh);
            emit_binop_imm(c, LIR_SAR, signed_value, left, sh);
            return signed_value;
        }
        return value;
    }
    if (lv->direct_slot)
        return emit_load_slot(c, raw, ty, lv->offset);
    emit(c, (Instr){
        .op = LIR_LOAD, .dst = raw, .a = lir_mem(lv->addr, 0),
        .w = load_lir_width(ty), .sgn = load_sign(ty),
        .fpw = type_is_floating(ty) ? float_width(ty) : LIR_FP_NONE,
        .aux = store_width_bytes(ty),
    });
    return widen_loaded_value(c, raw, ty);
}

static void store_prepared_lvalue(LowerCtx *c, PreparedLvalue *lv, int value)
{
    Type *ty = lv->lhs->ty;

    if (lv->bitfield) {
        Member *m = lv->bitfield;
        long value_mask = (1L << m->bit_width) - 1;
        long field_mask = value_mask << m->bit_offset;
        int cleared = fresh(c);
        int bits = fresh(c);
        int shifted = fresh(c);
        int merged = fresh(c);
        int truncated = fresh(c);

        emit_binop_imm(c, LIR_AND, cleared, lv->bitfield_unit, ~field_mask);
        emit_binop_imm(c, LIR_AND, bits, value, value_mask);
        emit_binop_imm(c, LIR_SHL, shifted, bits, m->bit_offset);
        emit(c, (Instr){
            .op = LIR_OR, .dst = merged, .a = lir_vreg(cleared),
            .b = lir_vreg(shifted), .w = LIR_W8,
        });
        emit(c, (Instr){
            .op = LIR_CONV, .dst = truncated, .a = lir_vreg(merged),
            .conv = CONV_TRUNC_LO32,
        });
        emit(c, (Instr){
            .op = LIR_STORE, .a = lir_mem(lv->addr, 0),
            .b = lir_vreg(truncated), .w = LIR_W4, .aux = 4,
        });
        return;
    }
    if (lv->direct_slot) {
        emit_store_slot(c, value, ty, lv->offset);
        return;
    }
    emit(c, (Instr){
        .op = LIR_STORE, .a = lir_mem(lv->addr, 0), .b = lir_vreg(value),
        .w = store_lir_width(ty),
        .fpw = type_is_floating(ty) ? float_width(ty) : LIR_FP_NONE,
        .aux = store_width_bytes(ty),
    });
}

static int lower_step_value(LowerCtx *c, int old, Type *oty, int is_inc)
{
    if (type_is_pointer(oty)) {
        int dst = fresh(c);
        long delta = (long)ptr_elem_size(oty) * (is_inc ? 1 : -1);
        if (fits_imm32(delta)) {
            emit(c, (Instr){
                .op = LIR_LEA,
                .dst = dst,
                .a = lir_mem(old, delta),
            });
        } else {
            int t = fresh(c);
            emit(c, (Instr){ .op = LIR_MOVI, .dst = t, .a = lir_imm(delta) });
            emit(c, (Instr){
                .op = LIR_ADD,
                .dst = dst,
                .a = lir_vreg(old),
                .b = lir_vreg(t),
                .w = LIR_W8,
            });
        }
        return dst;
    }
    if (type_is_floating(oty)) {
        int one;
        int dst = fresh_type(c, oty);
        double d = 1.0;
        float f = 1.0f;
        unsigned long bits = 0;

        if (float_width(oty) == LIR_FP_F80) {
            int integer_one = fresh(c);
            emit(c, (Instr){
                .op = LIR_MOVI, .dst = integer_one, .a = lir_imm(1),
            });
            one = emit_conv_value(c, integer_one, CONV_SI32_F80);
        } else {
            one = fresh_type(c, oty);
            if (float_width(oty) == LIR_FP_F32)
                memcpy(&bits, &f, sizeof(f));
            else
                memcpy(&bits, &d, sizeof(d));
            emit(c, (Instr){
                .op = LIR_FMOVI, .dst = one, .a = lir_imm((long)bits),
                .fpw = float_width(oty),
            });
        }
        emit(c, (Instr){ .op = is_inc ? LIR_FADD : LIR_FSUB, .dst = dst,
                         .a = lir_vreg(old), .b = lir_vreg(one),
                         .fpw = float_width(oty) });
        return dst;
    }
    {
        int dst = fresh(c);
        LirWidth w = store_lir_width(oty);
        emit(c, (Instr){
            .op = is_inc ? LIR_ADD : LIR_SUB,
            .dst = dst,
            .a = lir_vreg(old),
            .b = lir_imm(1),
            .w = w,
        });
        return dst;
    }
}

static int lower_incdec(LowerCtx *c, Node *n)
{
    Node *lv = n->operand;
    int is_post = n->kind == ND_POSTINC || n->kind == ND_POSTDEC;
    int is_inc = n->kind == ND_PREINC || n->kind == ND_POSTINC;
    PreparedLvalue prepared = prepare_lvalue(c, lv);
    int old = load_prepared_lvalue(c, &prepared);
    int updated = lower_step_value(c, old, lv->ty, is_inc);
    store_prepared_lvalue(c, &prepared, updated);
    if (is_post)
        return widen_loaded_value(c, old, n->ty);
    return widen_loaded_value(c, updated, n->ty);
}

static int lower_compound_assign(LowerCtx *c, Node *n)
{
    PreparedLvalue lv = prepare_lvalue(c, n->lhs);
    int old = load_prepared_lvalue(c, &lv);
    int left = convert_value(c, old, n->lhs->ty, n->op_ty);
    int right = lower_expr(c, n->rhs);
    int result = fresh_type(c, n->op_ty);

    if (type_is_pointer(n->op_ty)) {
        int scale = ptr_elem_size(n->op_ty);

        if (scale > 1) {
            int scaled = fresh(c);
            emit(c, (Instr){
                .op = LIR_MUL, .dst = scaled, .a = lir_vreg(right),
                .b = lir_imm(scale), .w = LIR_W8,
            });
            right = scaled;
        }
        if (n->op == OP_ADD) {
            emit(c, (Instr){
                .op = LIR_LEA, .dst = result,
                .a = lir_mem_idx(left, right, 1, 0),
            });
        } else {
            emit(c, (Instr){
                .op = LIR_SUB, .dst = result, .a = lir_vreg(left),
                .b = lir_vreg(right), .w = LIR_W8,
            });
        }
    } else if (type_is_floating(n->op_ty)) {
        LirOp op = n->op == OP_ADD ? LIR_FADD :
                   n->op == OP_SUB ? LIR_FSUB :
                   n->op == OP_MUL ? LIR_FMUL : LIR_FDIV;
        emit(c, (Instr){ .op = op, .dst = result,
                         .a = lir_vreg(left), .b = lir_vreg(right),
                         .fpw = float_width(n->op_ty) });
    } else {
        LirWidth w = type_int_width(n->op_ty) == 8 ? LIR_W8 : LIR_W4;
        LirSign sgn = type_is_unsigned(n->op_ty) ? LIR_SGN_U : LIR_SGN_S;
        LirOp op;

        switch (n->op) {
        case OP_ADD:    op = LIR_ADD; break;
        case OP_SUB:    op = LIR_SUB; break;
        case OP_MUL:    op = LIR_MUL; break;
        case OP_DIV:    op = LIR_DIV; break;
        case OP_MOD:    op = LIR_MOD; break;
        case OP_SHL:    op = LIR_SHL; break;
        case OP_SHR:    op = sgn == LIR_SGN_U ? LIR_SHR : LIR_SAR; break;
        case OP_BITAND: op = LIR_AND; break;
        case OP_BITXOR: op = LIR_XOR; break;
        case OP_BITOR:  op = LIR_OR; break;
        default:        assert(0); op = LIR_ADD; break;
        }
        emit(c, (Instr){
            .op = op, .dst = result, .a = lir_vreg(left),
            .b = lir_vreg(right), .w = w, .sgn = sgn,
        });
    }

    result = convert_value(c, result, n->op_ty, n->lhs->ty);
    store_prepared_lvalue(c, &lv, result);
    return widen_loaded_value(c, result, n->ty);
}

static int lower_expr(LowerCtx *c, Node *n)
{
    if (n->func_decay && n->kind == ND_VAR) {
        int dst = fresh(c);

        emit(c, (Instr){
            .op = LIR_LEA_SYM,
            .dst = dst,
            .sym_name = node_symbol_name(n),
        });
        return dst;
    }
    if (n->var_decay)
        return lower_addr(c, n);

    switch (n->kind) {
    case ND_NUM: {
        int dst = fresh_type(c, n->ty);
        if (n->is_floating_literal) {
            long bits;
            if (float_width(n->ty) == LIR_FP_F32) {
                float value = (float)n->float_val;
                unsigned int raw;
                memcpy(&raw, &value, sizeof(raw));
                bits = (long)raw;
            } else if (float_width(n->ty) == LIR_FP_F64) {
                double value = (double)n->float_val;
                unsigned long raw;
                memcpy(&raw, &value, sizeof(raw));
                bits = (long)raw;
            } else {
                bits = 0;
            }
            emit(c, (Instr){ .op = LIR_FMOVI, .dst = dst,
                             .a = lir_imm(bits), .fpw = float_width(n->ty),
                             .fimm = n->float_val });
        } else {
            emit(c, (Instr){ .op = LIR_MOVI, .dst = dst, .a = lir_imm(n->val) });
        }
        return dst;
    }
    case ND_STRING:
        return lower_symbol_addr(c, n->string_label);
    case ND_VAR: {
        if (n->storage == VAR_STORAGE_GLOBAL) {
            int addr = lower_symbol_addr(c, node_symbol_name(n));
            int dst = fresh_type(c, n->ty);
            emit(c, (Instr){
                .op = LIR_LOAD,
                .dst = dst,
                .a = lir_mem(addr, 0),
                .w = load_lir_width(n->ty),
                .sgn = load_sign(n->ty),
                .fpw = type_is_floating(n->ty) ? float_width(n->ty) : LIR_FP_NONE,
                .aux = store_width_bytes(n->ty),
            });
            return widen_loaded_value(c, dst, n->ty);
        }
        int dst = fresh_type(c, n->ty);
        return emit_load_slot(c, dst, n->ty, n->offset);
    }
    case ND_CALL:
        if (n->func_ty && abi_type_is_record_pass(n->func_ty->ret))
            return lower_call_ex(c, n, CALL_DEST_NONE);
        return lower_call(c, n);
    case ND_POS:
        return lower_expr(c, n->operand);
    case ND_NEG: {
        int v = lower_expr(c, n->operand);
        int dst = fresh_type(c, n->ty);
        emit(c, (Instr){
            .op = type_is_floating(n->ty) ? LIR_FNEG : LIR_NEG,
            .dst = dst, .a = lir_vreg(v), .w = expr_width(n->operand),
            .fpw = type_is_floating(n->ty) ? float_width(n->ty) : LIR_FP_NONE });
        return dst;
    }
    case ND_BITNOT: {
        int v = lower_expr(c, n->operand);
        int dst = fresh_type(c, n->ty);
        emit(c, (Instr){
            .op = LIR_XOR, .dst = dst, .a = lir_vreg(v), .b = lir_imm(-1),
            .w = expr_width(n->operand),
        });
        return dst;
    }
    case ND_PREINC:
    case ND_PREDEC:
    case ND_POSTINC:
    case ND_POSTDEC:
        return lower_incdec(c, n);
    case ND_NOT: {
        int v = lower_expr(c, n->operand);
        int dst = fresh_type(c, n->ty);
        if (type_is_floating(n->operand->ty)) {
            int zero = fresh_type(c, n->operand->ty);
            emit(c, (Instr){ .op = LIR_FMOVI, .dst = zero, .a = lir_imm(0),
                             .fpw = float_width(n->operand->ty) });
            emit(c, (Instr){ .op = LIR_FSETCC, .dst = dst,
                             .a = lir_vreg(v), .b = lir_vreg(zero),
                             .fpw = float_width(n->operand->ty), .cc = CC_EQ });
        } else {
            emit(c, (Instr){
                .op = LIR_SETCC, .dst = dst, .a = lir_vreg(v),
                .b = lir_imm(0), .w = expr_width(n->operand), .cc = CC_EQ,
            });
        }
        return dst;
    }
    case ND_LOGAND:
    case ND_LOGOR: {
        int yes = lir_new_label(c->lf);
        int no = lir_new_label(c->lf);
        int merge = lir_new_label(c->lf);
        int dst = fresh_type(c, n->ty);
        int one = fresh(c);
        int zero = fresh(c);
        LirBlockId yes_exit;
        LirBlockId no_exit;

        lower_branch(c, n, yes, no);
        emit(c, (Instr){ .op = LIR_LABEL, .label = yes });
        emit(c, (Instr){ .op = LIR_MOVI, .dst = one, .a = lir_imm(1) });
        yes_exit = c->current;
        emit(c, (Instr){ .op = LIR_JMP, .label = merge });

        emit(c, (Instr){ .op = LIR_LABEL, .label = no });
        emit(c, (Instr){ .op = LIR_MOVI, .dst = zero, .a = lir_imm(0) });
        no_exit = c->current;
        emit(c, (Instr){ .op = LIR_JMP, .label = merge });

        emit(c, (Instr){ .op = LIR_LABEL, .label = merge });
        LirPhi *phi = lir_block_add_phi(current_block(c), dst);
        lir_phi_add_input(phi, yes_exit, one);
        lir_phi_add_input(phi, no_exit, zero);
        return dst;
    }
    case ND_COND: {
        int yes = lir_new_label(c->lf);
        int no = lir_new_label(c->lf);
        int merge = lir_new_label(c->lf);
        int dst = fresh_type(c, n->ty);
        int yes_value;
        int no_value;
        LirBlockId yes_exit;
        LirBlockId no_exit;

        lower_branch(c, n->cond, yes, no);
        emit(c, (Instr){ .op = LIR_LABEL, .label = yes });
        yes_value = lower_expr(c, n->then_expr);
        yes_exit = c->current;
        emit(c, (Instr){ .op = LIR_JMP, .label = merge });

        emit(c, (Instr){ .op = LIR_LABEL, .label = no });
        no_value = lower_expr(c, n->else_expr);
        no_exit = c->current;
        emit(c, (Instr){ .op = LIR_JMP, .label = merge });

        emit(c, (Instr){ .op = LIR_LABEL, .label = merge });
        LirPhi *phi = lir_block_add_phi(current_block(c), dst);
        lir_phi_add_input(phi, yes_exit, yes_value);
        lir_phi_add_input(phi, no_exit, no_value);
        return dst;
    }
    case ND_ADDR: {
        int dst = fresh(c);
        switch (n->operand->kind) {
        case ND_STRING:
            return lower_symbol_addr(c, n->operand->string_label);
        case ND_VAR:
            if (n->operand->ty && n->operand->ty->kind == TY_FUNC) {
                return lower_symbol_addr(c, node_symbol_name(n->operand));
            }
            if (n->operand->storage == VAR_STORAGE_GLOBAL)
                return lower_symbol_addr(c, node_symbol_name(n->operand));
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
        int dst = fresh_type(c, n->ty);
        int bytes = store_width_bytes(n->ty);
        emit(c, (Instr){
            .op = LIR_LOAD,
            .dst = dst,
            .a = lir_mem(addr, 0),
            .w = load_lir_width(n->ty),
            .sgn = load_sign(n->ty),
            .fpw = type_is_floating(n->ty) ? float_width(n->ty) : LIR_FP_NONE,
            .aux = bytes,
        });
        return widen_loaded_value(c, dst, n->ty);
    }
    case ND_MEMBER: {
        Member *m = member_meta(n);
        if (m->is_bitfield && m->bit_width > 0)
            return lower_bitfield_load(c, n, m);
        int addr = lower_member_addr(c, n);
        int dst = fresh_type(c, n->ty);
        int bytes = store_width_bytes(n->ty);
        emit(c, (Instr){
            .op = LIR_LOAD,
            .dst = dst,
            .a = lir_mem(addr, 0),
            .w = load_lir_width(n->ty),
            .sgn = load_sign(n->ty),
            .fpw = type_is_floating(n->ty) ? float_width(n->ty) : LIR_FP_NONE,
            .aux = bytes,
        });
        return widen_loaded_value(c, dst, n->ty);
    }
    case ND_CAST: {
        return lower_cast_value(c, n);
    }
    case ND_ASSIGN: {
        if (n->is_compound_assign)
            return lower_compound_assign(c, n);
        if (type_is_record(n->lhs->ty)) {
            if (n->rhs->kind == ND_CALL &&
                abi_type_is_record_pass(n->lhs->ty) &&
                n->lhs->kind == ND_VAR &&
                n->lhs->storage == VAR_STORAGE_LOCAL) {
                int val = lower_call_ex(c, n->rhs, n->lhs->offset);
                return val;
            }
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
            if (n->lhs->storage == VAR_STORAGE_GLOBAL) {
                int addr = lower_symbol_addr(c, node_symbol_name(n->lhs));
                emit(c, (Instr){
                    .op = LIR_STORE,
                    .a = lir_mem(addr, 0),
                    .b = lir_vreg(val),
                    .w = store_lir_width(n->lhs->ty),
                    .fpw = type_is_floating(n->lhs->ty) ? float_width(n->lhs->ty) : LIR_FP_NONE,
                    .aux = store_width_bytes(n->lhs->ty),
                });
            } else {
                emit_store_slot(c, val, n->lhs->ty, n->lhs->offset);
            }
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
                .fpw = type_is_floating(n->lhs->ty) ? float_width(n->lhs->ty) : LIR_FP_NONE,
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
            .fpw = type_is_floating(n->lhs->ty) ? float_width(n->lhs->ty) : LIR_FP_NONE,
            .aux = store_width_bytes(n->lhs->ty),
        });
        return val;
    }
    case ND_BINOP: {
        int dst = fresh_type(c, n->ty);
        lower_binop(c, dst, n->op, n->lhs, n->rhs, n->ty);
        return dst;
    }
    default:
        assert(0);
        return fresh(c);
    }
}

static void lower_branch(LowerCtx *c, Node *cond, int true_label,
                         int false_label)
{
    if (cond->kind == ND_NOT) {
        lower_branch(c, cond->operand, false_label, true_label);
        return;
    }
    if (cond->kind == ND_LOGAND) {
        int rhs = lir_new_label(c->lf);
        lower_branch(c, cond->lhs, rhs, false_label);
        emit(c, (Instr){ .op = LIR_LABEL, .label = rhs });
        lower_branch(c, cond->rhs, true_label, false_label);
        return;
    }
    if (cond->kind == ND_LOGOR) {
        int rhs = lir_new_label(c->lf);
        lower_branch(c, cond->lhs, true_label, rhs);
        emit(c, (Instr){ .op = LIR_LABEL, .label = rhs });
        lower_branch(c, cond->rhs, true_label, false_label);
        return;
    }
    if (cond->kind == ND_BINOP && is_cmp(cond->op)) {
        if (type_is_floating(cond->lhs->ty)) {
            int vl = lower_expr(c, cond->lhs);
            int vr = lower_expr(c, cond->rhs);
            emit(c, (Instr){
                .op = LIR_BR, .a = lir_vreg(vl), .b = lir_vreg(vr),
                .fpw = float_width(cond->lhs->ty), .cc = binop_cc(cond->op),
                .label = true_label,
            });
            emit(c, (Instr){ .op = LIR_JMP, .label = false_label });
            return;
        }
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
            .cc = binop_cc(cond->op),
            .sgn = binop_sign(cond->lhs, cond->rhs, cond->ty, cond->op),
            .label = true_label,
        });
        emit(c, (Instr){ .op = LIR_JMP, .label = false_label });
        return;
    }

    int v = lower_expr(c, cond);
    if (type_is_floating(cond->ty)) {
        int zero = fresh_type(c, cond->ty);
        emit(c, (Instr){ .op = LIR_FMOVI, .dst = zero, .a = lir_imm(0),
                         .fpw = float_width(cond->ty) });
        emit(c, (Instr){
            .op = LIR_BR, .a = lir_vreg(v), .b = lir_vreg(zero),
            .fpw = float_width(cond->ty), .cc = CC_NE, .label = true_label,
        });
        emit(c, (Instr){ .op = LIR_JMP, .label = false_label });
        return;
    }
    emit(c, (Instr){
        .op = LIR_BR,
        .a = lir_vreg(v),
        .b = lir_imm(0),
        .w = expr_width(cond),
        .cc = CC_NE,
        .label = true_label,
    });
    emit(c, (Instr){ .op = LIR_JMP, .label = false_label });
}

static void lower_stmt(LowerCtx *c, Node *n)
{
    switch (n->kind) {
    case ND_RETURN: {
        if (n->operand && abi_type_is_record_pass(c->fn->ret_ty)) {
            lower_return_record(c, n->operand, c->fn->ret_ty);
            return;
        }
        if (n->operand) {
            int v = lower_expr(c, n->operand);
            if (type_is_floating(c->fn->ret_ty) &&
                float_width(c->fn->ret_ty) == LIR_FP_F80) {
                emit(c, (Instr){
                    .op = LIR_FRET, .dst = LIR_NO_VREG,
                    .a = lir_vreg(v), .fpw = LIR_FP_F80,
                });
            } else {
                emit(c, (Instr){
                    .op = LIR_MOV,
                    .dst = LIR_NO_VREG,
                    .a = lir_vreg(v),
                    .b = lir_phys(type_is_floating(c->fn->ret_ty)
                                  ? PHYS_XMM0 : PHYS_RAX),
                    .fpw = type_is_floating(c->fn->ret_ty)
                         ? float_width(c->fn->ret_ty) : LIR_FP_NONE,
                });
            }
        }
        emit(c, (Instr){ .op = LIR_JMP, .label = c->ret_label });
        return;
    }
    case ND_EXPR_STMT:
        if (n->operand)
            (void)lower_expr(c, n->operand);
        return;
    case ND_DECL:
        if ((n->decl_storage != STORAGE_NONE &&
             n->decl_storage != STORAGE_AUTO &&
             n->decl_storage != STORAGE_REGISTER) ||
            (n->ty && n->ty->kind == TY_FUNC))
            return;
        if (n->init && n->init->kind == ND_CALL &&
            abi_type_is_record_pass(n->ty)) {
            (void)lower_call_ex(c, n->init, n->offset);
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
        int then_id = lir_new_label(c->lf);
        int else_id = n->else_body ? lir_new_label(c->lf) : -1;
        int end_id = lir_new_label(c->lf);
        lower_branch(c, n->cond, then_id,
                     n->else_body ? else_id : end_id);
        emit(c, (Instr){ .op = LIR_LABEL, .label = then_id });
        lower_stmt(c, n->then_body);
        if (n->else_body) {
            emit(c, (Instr){ .op = LIR_JMP, .label = end_id });
            emit(c, (Instr){ .op = LIR_LABEL, .label = else_id });
            lower_stmt(c, n->else_body);
            emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        } else {
            emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        }
        return;
    }
    case ND_WHILE: {
        int begin_id = lir_new_label(c->lf);
        int body_id = lir_new_label(c->lf);
        int end_id = lir_new_label(c->lf);
        ControlCtx loop;
        control_push(c, &loop, begin_id, end_id);
        emit(c, (Instr){ .op = LIR_LABEL, .label = begin_id });
        lower_branch(c, n->cond, body_id, end_id);
        emit(c, (Instr){ .op = LIR_LABEL, .label = body_id });
        lower_stmt(c, n->then_body);
        emit(c, (Instr){ .op = LIR_JMP, .label = begin_id });
        emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        control_pop(c);
        return;
    }
    case ND_DO_WHILE: {
        int body_id = lir_new_label(c->lf);
        int cond_id = lir_new_label(c->lf);
        int end_id = lir_new_label(c->lf);
        ControlCtx loop;
        control_push(c, &loop, cond_id, end_id);
        emit(c, (Instr){ .op = LIR_LABEL, .label = body_id });
        lower_stmt(c, n->then_body);
        emit(c, (Instr){ .op = LIR_LABEL, .label = cond_id });
        lower_branch(c, n->cond, body_id, end_id);
        emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        control_pop(c);
        return;
    }
    case ND_FOR: {
        int begin_id = lir_new_label(c->lf);
        int body_id = lir_new_label(c->lf);
        int step_id = lir_new_label(c->lf);
        int end_id = lir_new_label(c->lf);
        ControlCtx loop;
        if (n->init)
            lower_expr(c, n->init);
        control_push(c, &loop, step_id, end_id);
        emit(c, (Instr){ .op = LIR_LABEL, .label = begin_id });
        if (n->cond)
            lower_branch(c, n->cond, body_id, end_id);
        else
            emit(c, (Instr){ .op = LIR_JMP, .label = body_id });
        emit(c, (Instr){ .op = LIR_LABEL, .label = body_id });
        lower_stmt(c, n->then_body);
        emit(c, (Instr){ .op = LIR_LABEL, .label = step_id });
        if (n->step)
            lower_expr(c, n->step);
        emit(c, (Instr){ .op = LIR_JMP, .label = begin_id });
        emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        control_pop(c);
        return;
    }
    case ND_SWITCH: {
        int value = lower_expr(c, n->cond);
        int end_id = lir_new_label(c->lf);
        ControlCtx control;
        Node *case_node;

        for (case_node = n->cases; case_node;
             case_node = case_node->case_next)
            case_node->label = lir_new_label(c->lf);
        if (n->default_case)
            n->default_case->label = lir_new_label(c->lf);

        for (case_node = n->cases; case_node;
             case_node = case_node->case_next) {
            Operand case_value;
            if (fits_imm32(case_node->case_val)) {
                case_value = lir_imm(case_node->case_val);
            } else {
                int case_vreg = fresh(c);
                emit(c, (Instr){ .op = LIR_MOVI, .dst = case_vreg,
                                  .a = lir_imm(case_node->case_val) });
                case_value = lir_vreg(case_vreg);
            }
            emit(c, (Instr){
                .op = LIR_BR,
                .a = lir_vreg(value),
                .b = case_value,
                .w = expr_width(n->cond),
                .cc = CC_EQ,
                .label = case_node->label,
            });
        }
        emit(c, (Instr){
            .op = LIR_JMP,
            .label = n->default_case ? n->default_case->label : end_id,
        });

        control_push(c, &control, -1, end_id);
        lower_stmt(c, n->then_body);
        control_pop(c);
        emit(c, (Instr){ .op = LIR_JMP, .label = end_id });
        emit(c, (Instr){ .op = LIR_LABEL, .label = end_id });
        return;
    }
    case ND_CASE:
    case ND_DEFAULT:
        emit(c, (Instr){ .op = LIR_LABEL, .label = n->label });
        lower_stmt(c, n->then_body);
        return;
    case ND_BREAK:
        if (c->control)
            emit(c, (Instr){ .op = LIR_JMP,
                              .label = c->control->break_label });
        return;
    case ND_CONTINUE: {
        ControlCtx *control;
        for (control = c->control;
             control && control->continue_label < 0;
             control = control->prev)
            ;
        if (control)
            emit(c, (Instr){ .op = LIR_JMP,
                              .label = control->continue_label });
        return;
    }
    case ND_LABEL:
        emit(c, (Instr){ .op = LIR_LABEL,
                          .label = named_label_id(c, n) });
        lower_stmt(c, n->then_body);
        return;
    case ND_GOTO:
        emit(c, (Instr){ .op = LIR_JMP,
                          .label = named_label_id(c, n->goto_target) });
        return;
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
    case ND_LABEL:
        return stmt_returns(n->then_body);
    default:
        return 0;
    }
}

LirFn *lower_function(Function *fn)
{
    LirFn *lf = lir_fn_new(fn->name);
    lf->frame_locals = fn->frame_locals;
    lf->nframe_locals = fn->nframe_locals;
    LowerCtx ctx = {
        .lf = lf,
        .fn = fn,
        .ret_label = lir_new_label(lf),
        .current = lf->entry_block,
    };
    lf->epilogue_label = lir_label_block(lf, ctx.ret_label);

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

    for (int i = 0; i < lf->nblocks; i++) {
        if (lf->blocks[i].term.kind == LIR_TERM_NONE) {
            lf->blocks[i].term.kind = LIR_TERM_JMP;
            lf->blocks[i].term.target = lf->epilogue_label;
        }
    }

    return lf;
}
