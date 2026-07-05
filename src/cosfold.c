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

static long int_promote_literal_value(long v, Type *ty)
{
    Type *promoted = type_int_promote(ty);

    if (promoted == ty)
        return v;

    if (type_is_unsigned(ty)) {
        int w = type_int_width(ty);
        unsigned long uv = (unsigned long)v;

        if (w == 1)
            uv &= 0xFFUL;
        else if (w == 2)
            uv &= 0xFFFFUL;
        return (long)uv;
    }
    if (type_is_signed_char(ty))
        return (long)(signed char)(unsigned char)v;
    if (type_is_plain_char(ty) || type_is_unsigned_char(ty))
        return (long)((unsigned char)v);
    if (type_is_short(ty))
        return (long)(short)v;
    return v;
}

static int fold_literal_operand(Node *n, long *out, Type **out_ty)
{
    if (!n || !out || !out_ty)
        return 0;

    if (n->kind == ND_NUM) {
        *out = n->val;
        *out_ty = n->ty;
        return 1;
    }
    if (n->kind == ND_CAST && n->operand && n->operand->kind == ND_NUM &&
        type_is_integer(n->ty)) {
        *out = type_convert_const(n->operand->val, n->ty);
        *out_ty = n->ty;
        return 1;
    }
    return 0;
}

static Type *fold_binop_ty(Node *n, Type *lty, Type *rty)
{
    Type *la = type_int_promote(lty);
    Type *rb = type_int_promote(rty);

    (void)n;
    return type_arith_convert(la, rb);
}

static int foldable_binop(Node *n)
{
    long lv, rv;
    Type *lty, *rty;

    if (!n || n->kind != ND_BINOP)
        return 0;
    if (!n->lhs || !n->rhs)
        return 0;
    if (!fold_literal_operand(n->lhs, &lv, &lty) ||
        !fold_literal_operand(n->rhs, &rv, &rty))
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
    case ND_BINOP: {
        long lv, rv;
        Type *lty, *rty;

        changed |= cosfold_expr(&n->lhs);
        changed |= cosfold_expr(&n->rhs);
        if (foldable_binop(n) &&
            fold_literal_operand(n->lhs, &lv, &lty) &&
            fold_literal_operand(n->rhs, &rv, &rty)) {
            long v;
            Type *fty;

            lv = int_promote_literal_value(lv, lty);
            rv = int_promote_literal_value(rv, rty);
            fty = fold_binop_ty(n, lty, rty);
            if (int_const_binop_ty(n->op, lv, rv, fty, &v)) {
                *np = fold_to_num(v, n->ty, n->loc);
                return 1;
            }
        }
        return changed;
    }
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
            long v = type_convert_const(n->operand->val, n->ty);
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
