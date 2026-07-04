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
    if (type_is_pointer(lhs->ty) || type_is_pointer(rhs->ty))
        return LIR_W8;
    if (type_is_long(lhs->ty) || type_is_long(rhs->ty))
        return LIR_W8;
    return LIR_W4;
}

static LirWidth expr_width(Node *n)
{
    if (!n || !n->ty)
        return LIR_W4;
    if (type_is_pointer(n->ty) || type_is_long(n->ty))
        return LIR_W8;
    return LIR_W4;
}

static int store_width_bytes(Type *ty)
{
    if (type_is_integer(ty))
        return type_int_width(ty);
    return 8;
}

static LirWidth store_lir_width(Type *ty)
{
    int w = store_width_bytes(ty);
    return w == 8 ? LIR_W8 : LIR_W4;
}

static LirSign load_sign(Type *ty)
{
    if (type_is_plain_char(ty) || type_is_unsigned(ty))
        return LIR_SGN_Z;
    return LIR_SGN_S;
}

static LirSign arith_sign(Type *ty)
{
    if (ty && type_is_unsigned(ty))
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
    if (type_is_long(ty) || type_is_pointer(ty))
        return LIR_W8;
    return LIR_W4;
}

static void emit_widen_to_rax(LowerCtx *c, int v, Type *ty)
{
    if (load_lir_width(ty) == LIR_W8)
        return;
    if (type_is_char(ty))
        return;
    if (type_is_short(ty)) {
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

static int ptr_elem_size(Type *ptr_ty)
{
    Type *elem = type_ptr_elem(ptr_ty);
    return elem ? type_size(elem) : 1;
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
static void lower_stmt(LowerCtx *c, Node *n);

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
    default:
        assert(0 && "invalid lvalue");
        return fresh(c);
    }
}

static void lower_cast_into(LowerCtx *c, int dst, Node *n)
{
    int v = lower_expr(c, n->operand);
    Type *to = n->ty;
    Type *from = n->operand->ty;

    emit(c, (Instr){ .op = LIR_MOV, .dst = dst, .a = lir_vreg(v) });

    if (type_is_void(to))
        return;

    if (type_is_char(to) && !type_is_char(from)) {
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_ZEXT8 });
        return;
    }
    if (type_is_short(to) && type_is_unsigned(to) && !type_is_short(from))
        return;
    if (type_is_short(to) && type_is_signed(to) && !type_is_short(from)) {
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_SEXT16 });
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_SEXT32_64 });
        return;
    }
    if (type_is_long(to) && type_is_integer(from) &&
        type_int_width(from) == 4) {
        if (type_is_unsigned(to)) {
            emit(c, (Instr){
                .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst),
                .conv = CONV_TRUNC_LO32 });
            return;
        }
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_SEXT32_64 });
        return;
    }
    if (type_is_integer(to) && type_int_width(to) == 4 &&
        (type_is_pointer(from) ||
         (type_is_integer(from) && type_int_width(from) == 8))) {
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_TRUNC_LO32 });
        emit(c, (Instr){
            .op = LIR_CONV, .dst = dst, .a = lir_vreg(dst), .conv = CONV_SEXT32_64 });
    }
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
        int vs = fresh(c);
        emit(c, (Instr){ .op = LIR_MOVI, .dst = vs, .a = lir_imm(scale) });
        int t = fresh(c);
        emit(c, (Instr){
            .op = LIR_MUL, .dst = t, .a = lir_vreg(vi), .b = lir_vreg(vs), .w = LIR_W8 });
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

static void lower_setcc(LowerCtx *c, int dst, BinOp op, int lhs, int rhs,
                        int w, LirSign sgn)
{
    emit(c, (Instr){
        .op = LIR_SETCC,
        .dst = dst,
        .a = lir_vreg(lhs),
        .b = lir_vreg(rhs),
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
    int vr = lower_expr(c, rhs);
    LirSign sgn = binop_sign(lhs, rhs, res_ty, op);

    if (is_cmp(op) && rhs->kind == ND_NUM && rhs->val == 0) {
        lower_setcc(c, dst, op, vl, vr, w, sgn);
        return;
    }

    switch (op) {
    case OP_ADD:
        emit(c, (Instr){
            .op = LIR_ADD, .dst = dst, .a = lir_vreg(vl), .b = lir_vreg(vr),
            .w = (LirWidth)w });
        return;
    case OP_SUB:
        emit(c, (Instr){
            .op = LIR_SUB, .dst = dst, .a = lir_vreg(vl), .b = lir_vreg(vr),
            .w = (LirWidth)w });
        return;
    case OP_MUL:
        emit(c, (Instr){
            .op = LIR_MUL, .dst = dst, .a = lir_vreg(vl), .b = lir_vreg(vr),
            .w = (LirWidth)w });
        return;
    case OP_DIV:
        emit(c, (Instr){
            .op = LIR_DIV, .dst = dst, .a = lir_vreg(vl), .b = lir_vreg(vr),
            .w = (LirWidth)w, .sgn = sgn });
        return;
    case OP_MOD:
        emit(c, (Instr){
            .op = LIR_MOD, .dst = dst, .a = lir_vreg(vl), .b = lir_vreg(vr),
            .w = (LirWidth)w, .sgn = sgn });
        return;
    case OP_EQ:
    case OP_NE:
    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
        lower_setcc(c, dst, op, vl, vr, w, sgn);
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
    case ND_CAST: {
        int dst = fresh(c);
        lower_cast_into(c, dst, n);
        return dst;
    }
    case ND_ASSIGN: {
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
        int vr = lower_expr(c, cond->rhs);
        emit(c, (Instr){
            .op = LIR_BR,
            .a = lir_vreg(vl),
            .b = lir_vreg(vr),
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
