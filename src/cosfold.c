/* SPDX-License-Identifier: MIT */
#include "cosfold.h"

#include "intconst.h"
#include "type.h"

static Node *fold_to_num(long v, Type *ty, SourceLoc loc)
{
    Node *n = node_num(v, loc);

    n->ty = ty;
    if (type_is_long(ty))
        n->has_long_suffix = 1;
    return n;
}

static long cast_fold_value(long v, Type *ty)
{
    if (type_is_unsigned(ty)) {
        int w = type_int_width(ty);
        if (w == 1)
            return (long)((unsigned long)v & 0xFFUL);
        if (w == 2)
            return (long)((unsigned long)v & 0xFFFFUL);
        if (w == 4)
            return (long)(unsigned int)v;
        return (long)(unsigned long)v;
    }
    if (type_is_char(ty))
        return (long)((unsigned long)v & 0xFFUL);
    if (type_is_short(ty))
        return (long)(short)v;
    if (type_is_integer(ty) && type_int_width(ty) == 4)
        return (long)(int)v;
    return v;
}

static int is_cmp_op(BinOp op)
{
    return op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_LE ||
           op == OP_GT || op == OP_GE;
}

static Type *fold_binop_ty(Node *n)
{
    if (is_cmp_op(n->op))
        return type_arith_convert(n->lhs->ty, n->rhs->ty);
    return n->ty;
}

static int foldable_binop(Node *n)
{
    if (!n || n->kind != ND_BINOP)
        return 0;
    if (!n->lhs || !n->rhs)
        return 0;
    if (n->lhs->kind != ND_NUM || n->rhs->kind != ND_NUM)
        return 0;
    return type_is_integer(n->ty);
}

int cosfold_expr(Node **np)
{
    Node *n = *np;
    int changed = 0;

    if (!n)
        return 0;

    switch (n->kind) {
    case ND_BINOP:
        changed |= cosfold_expr(&n->lhs);
        changed |= cosfold_expr(&n->rhs);
        if (foldable_binop(n)) {
            long v;
            Type *fty = fold_binop_ty(n);
            if (int_const_binop_ty(n->op, n->lhs->val, n->rhs->val, fty, &v)) {
                *np = fold_to_num(v, n->ty, n->loc);
                return 1;
            }
        }
        return changed;
    case ND_NEG:
        changed |= cosfold_expr(&n->operand);
        if (n->operand && n->operand->kind == ND_NUM &&
            type_is_integer(n->ty)) {
            long v;
            if (int_const_neg_ty(n->operand->val, n->ty, &v)) {
                *np = fold_to_num(v, n->ty, n->loc);
                return 1;
            }
        }
        return changed;
    case ND_ADDR:
        changed |= cosfold_expr(&n->operand);
        return changed;
    case ND_DEREF:
        changed |= cosfold_expr(&n->operand);
        return changed;
    case ND_CAST:
        changed |= cosfold_expr(&n->operand);
        /* Fold a cast of a constant to a constant of the target type.
         * Plain (char) reduces modulo 256 (unsigned-byte model); (void) has
         * no value, so it is left for codegen to discard. */
        if (n->operand && n->operand->kind == ND_NUM &&
            !type_is_void(n->ty)) {
            long v = cast_fold_value(n->operand->val, n->ty);
            *np = fold_to_num(v, n->ty, n->loc);
            return 1;
        }
        return changed;
    case ND_ASSIGN:
        changed |= cosfold_expr(&n->lhs);
        changed |= cosfold_expr(&n->rhs);
        return changed;
    case ND_CALL:
        for (Node **ap = &n->args; *ap; ap = &(*ap)->next)
            changed |= cosfold_expr(ap);
        return changed;
    case ND_NUM:
    case ND_VAR:
        return 0;
    default:
        return changed;
    }
}

static int fold_stmt(Node *s);

static int fold_stmt_list(Node *body)
{
    int changed = 0;

    for (Node *s = body; s; s = s->next)
        changed |= fold_stmt(s);
    return changed;
}

static int fold_stmt(Node *s)
{
    int changed = 0;

    if (!s)
        return 0;

    switch (s->kind) {
    case ND_DECL:
        changed |= cosfold_expr(&s->init);
        return changed;
    case ND_RETURN:
    case ND_EXPR_STMT:
        changed |= cosfold_expr(&s->operand);
        return changed;
    case ND_IF:
        changed |= cosfold_expr(&s->cond);
        changed |= fold_stmt(s->then_body);
        changed |= fold_stmt(s->else_body);
        return changed;
    case ND_WHILE:
        changed |= cosfold_expr(&s->cond);
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_FOR:
        changed |= cosfold_expr(&s->init);
        changed |= cosfold_expr(&s->cond);
        changed |= cosfold_expr(&s->step);
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_BLOCK:
        return fold_stmt_list(s->body);
    default:
        return 0;
    }
}

int cosfold_function(Function *fn)
{
    if (!fn || !fn->body)
        return 0;
    return fold_stmt_list(fn->body);
}

/* UNDEFER: fold mixed signed/unsigned binops when only literals differ in ty. */
/* UNDEFER: fold unsigned char/short promotions before the operation. */
