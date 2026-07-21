/* SPDX-License-Identifier: MIT */
#include "ast_const_fold.h"

#include "intconst.h"
#include "type.h"

#include <string.h>

static Node *fold_to_num(long v, Type *ty, SourceLoc loc)
{
    Node *n = node_num(v, loc);

    n->ty = ty;
    if (type_is_long(ty))
        n->has_long_suffix = 1;
    return n;
}

static void replace_expr_preserve_next(Node **np, Node *old, Node *newn)
{
    newn->next = old->next;
    *np = newn;
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
        *out = type_convert_const_from(n->operand->val, n->operand->ty, n->ty);
        *out_ty = n->ty;
        return 1;
    }
    return 0;
}

static Type *fold_binop_ty(Node *n, Type *lty, Type *rty)
{
    Type *la = type_int_promote(lty);
    Type *rb = type_int_promote(rty);

    if (n->op == OP_SHL || n->op == OP_SHR)
        return la;
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

static int same_scalar_variable(Node *lhs, Node *rhs)
{
    return lhs && rhs && lhs->kind == ND_VAR && rhs->kind == ND_VAR &&
           type_is_scalar(lhs->ty) && type_is_scalar(rhs->ty) &&
           lhs->offset == rhs->offset && lhs->name && rhs->name &&
           strcmp(lhs->name, rhs->name) == 0;
}

int ast_const_fold_expr(Node **np)
{
    Node *n = *np;
    int changed = 0;

    if (!n)
        return 0;

    switch (n->kind) {
    case ND_BINOP: {
        long lv, rv;
        Type *lty, *rty;

        changed |= ast_const_fold_expr(&n->lhs);
        changed |= ast_const_fold_expr(&n->rhs);
        if (n->op == OP_COMMA && n->lhs && n->lhs->kind == ND_NUM) {
            replace_expr_preserve_next(np, n, n->rhs);
            return 1;
        }
        /* x - x is zero for the same non-volatile scalar object.  xcc does
           not yet support volatile-qualified types; add that guard here when
           qualifiers are introduced. */
        if (n->op == OP_SUB && same_scalar_variable(n->lhs, n->rhs)) {
            replace_expr_preserve_next(np, n, fold_to_num(0, n->ty, n->loc));
            return 1;
        }
        if (foldable_binop(n) &&
            fold_literal_operand(n->lhs, &lv, &lty) &&
            fold_literal_operand(n->rhs, &rv, &rty)) {
            long v;
            Type *fty;

            lv = int_promote_literal_value(lv, lty);
            rv = int_promote_literal_value(rv, rty);
            fty = fold_binop_ty(n, lty, rty);
            if (int_const_binop_ty(n->op, lv, rv, fty, &v)) {
                replace_expr_preserve_next(np, n, fold_to_num(v, n->ty, n->loc));
                return 1;
            }
        }
        return changed;
    }
    case ND_NEG:
        changed |= ast_const_fold_expr(&n->operand);
        if (n->operand && n->operand->kind == ND_NUM &&
            type_is_integer(n->ty)) {
            long v;
            if (int_const_neg_ty(n->operand->val, n->ty, &v)) {
                replace_expr_preserve_next(np, n, fold_to_num(v, n->ty, n->loc));
                return 1;
            }
        }
        return changed;
    case ND_NOT:
        changed |= ast_const_fold_expr(&n->operand);
        if (n->operand && n->operand->kind == ND_NUM) {
            replace_expr_preserve_next(np, n,
                fold_to_num(!n->operand->val, type_int(), n->loc));
            return 1;
        }
        return changed;
    case ND_LOGAND:
    case ND_LOGOR:
        changed |= ast_const_fold_expr(&n->lhs);
        changed |= ast_const_fold_expr(&n->rhs);
        if (n->lhs && n->rhs && n->lhs->kind == ND_NUM &&
            n->rhs->kind == ND_NUM) {
            long value = n->kind == ND_LOGAND
                ? (n->lhs->val != 0 && n->rhs->val != 0)
                : (n->lhs->val != 0 || n->rhs->val != 0);
            replace_expr_preserve_next(np, n,
                fold_to_num(value, type_int(), n->loc));
            return 1;
        }
        return changed;
    case ND_COND:
        changed |= ast_const_fold_expr(&n->cond);
        changed |= ast_const_fold_expr(&n->then_expr);
        changed |= ast_const_fold_expr(&n->else_expr);
        if (n->cond && n->cond->kind == ND_NUM) {
            Node *selected = n->cond->val ? n->then_expr : n->else_expr;
            replace_expr_preserve_next(np, n, selected);
            return 1;
        }
        return changed;
    case ND_ADDR:
        changed |= ast_const_fold_expr(&n->operand);
        return changed;
    case ND_DEREF:
        changed |= ast_const_fold_expr(&n->operand);
        return changed;
    case ND_CAST:
        changed |= ast_const_fold_expr(&n->operand);
        /* Fold a cast of a constant to a constant of the target type.
         * Plain (char) reduces modulo 256 (unsigned-byte model); (void) has
         * no value, so it is left for codegen to discard. */
        if (n->operand && n->operand->kind == ND_NUM &&
            !type_is_void(n->ty) && type_is_scalar(n->ty)) {
            long v = type_convert_const_from(n->operand->val,
                                             n->operand->ty, n->ty);
            replace_expr_preserve_next(np, n, fold_to_num(v, n->ty, n->loc));
            return 1;
        }
        return changed;
    case ND_ASSIGN:
        changed |= ast_const_fold_expr(&n->lhs);
        changed |= ast_const_fold_expr(&n->rhs);
        return changed;
    case ND_CALL:
        changed |= ast_const_fold_expr(&n->callee);
        for (Node **ap = &n->args; *ap; ap = &(*ap)->next)
            changed |= ast_const_fold_expr(ap);
        return changed;
    case ND_INIT_LIST:
        for (Node **p = &n->body; *p; p = &(*p)->next)
            changed |= ast_const_fold_expr(p);
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
        if (s->init && s->init->kind == ND_INIT_LIST) {
            for (Node **p = &s->init->body; *p; p = &(*p)->next)
                changed |= ast_const_fold_expr(p);
        } else
            changed |= ast_const_fold_expr(&s->init);
        return changed;
    case ND_RETURN:
    case ND_EXPR_STMT:
        changed |= ast_const_fold_expr(&s->operand);
        return changed;
    case ND_IF:
        changed |= ast_const_fold_expr(&s->cond);
        changed |= fold_stmt(s->then_body);
        changed |= fold_stmt(s->else_body);
        return changed;
    case ND_WHILE:
    case ND_DO_WHILE:
        changed |= ast_const_fold_expr(&s->cond);
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_FOR:
        changed |= ast_const_fold_expr(&s->init);
        changed |= ast_const_fold_expr(&s->cond);
        changed |= ast_const_fold_expr(&s->step);
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_SWITCH:
        changed |= ast_const_fold_expr(&s->cond);
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_CASE:
        changed |= ast_const_fold_expr(&s->operand);
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_DEFAULT:
        changed |= fold_stmt(s->then_body);
        return changed;
    case ND_LABEL:
        return fold_stmt(s->then_body);
    case ND_GOTO:
        return 0;
    case ND_BLOCK:
        return fold_stmt_list(s->body);
    default:
        return 0;
    }
}

int ast_const_fold_function(Function *fn)
{
    if (!fn || !fn->body)
        return 0;
    return fold_stmt_list(fn->body);
}
