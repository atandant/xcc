/* SPDX-License-Identifier: MIT */
#include "cosfold.h"

#include "type.h"

static Node *fold_to_num(long v, Type *ty, SourceLoc loc)
{
    Node *n = node_num(v, loc);

    n->ty = ty;
    if (type_is_long(ty))
        n->has_long_suffix = 1;
    return n;
}

static int eval_binop(BinOp op, long a, long b, long *out)
{
    switch (op) {
    case OP_ADD: *out = a + b; return 1;
    case OP_SUB: *out = a - b; return 1;
    case OP_MUL: *out = a * b; return 1;
    case OP_DIV:
        if (b == 0)
            return 0;
        *out = a / b;
        return 1;
    case OP_MOD:
        if (b == 0)
            return 0;
        *out = a % b;
        return 1;
    case OP_EQ:  *out = a == b; return 1;
    case OP_NE:  *out = a != b; return 1;
    case OP_LT:  *out = a < b;  return 1;
    case OP_LE:  *out = a <= b; return 1;
    case OP_GT:  *out = a > b;  return 1;
    case OP_GE:  *out = a >= b; return 1;
    }
    return 0;
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
            if (eval_binop(n->op, n->lhs->val, n->rhs->val, &v)) {
                *np = fold_to_num(v, n->ty, n->loc);
                return 1;
            }
        }
        return changed;
    case ND_NEG:
        changed |= cosfold_expr(&n->operand);
        if (n->operand && n->operand->kind == ND_NUM &&
            type_is_integer(n->ty)) {
            *np = fold_to_num(-n->operand->val, n->ty, n->loc);
            return 1;
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
         * (char) reduces modulo 256 (unsigned-byte model); (void) has no
         * value, so it is left for codegen to discard. */
        if (n->operand && n->operand->kind == ND_NUM &&
            !type_is_void(n->ty)) {
            long v = n->operand->val;
            if (type_is_char(n->ty))
                v &= 0xFF;
            else if (type_is_integer(n->ty) &&
                     type_int_width(n->ty) == 4)
                v = (int)v;
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
