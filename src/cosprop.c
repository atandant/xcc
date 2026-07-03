/* SPDX-License-Identifier: MIT */
#include "cosprop.h"

#include "cosfold.h"
#include "type.h"

#include <string.h>

#define MAX_BINDS 1024

typedef struct {
    int offset;
    long val;
} Bind;

static Bind binds[MAX_BINDS];
static int nbinds;
static int scope_start;

typedef struct {
    Bind snap[MAX_BINDS];
    int n;
    int scope;
} BindSnap;

static void bind_snapshot(BindSnap *s)
{
    s->n = nbinds;
    s->scope = scope_start;
    memcpy(s->snap, binds, sizeof(Bind) * (size_t)nbinds);
}

static void bind_restore(const BindSnap *s)
{
    nbinds = s->n;
    scope_start = s->scope;
    memcpy(binds, s->snap, sizeof(Bind) * (size_t)s->n);
}

static void bind_clear(void)
{
    nbinds = 0;
    scope_start = 0;
}

static void bind_enter_scope(void)
{
    scope_start = nbinds;
}

static void bind_leave_scope(void)
{
    nbinds = scope_start;
}

static void bind_clear_offset(int offset)
{
    for (int i = 0; i < nbinds; i++) {
        if (binds[i].offset == offset) {
            binds[i] = binds[nbinds - 1];
            nbinds--;
            return;
        }
    }
}

static void bind_set(int offset, long val)
{
    bind_clear_offset(offset);
    if (nbinds >= MAX_BINDS)
        return;
    binds[nbinds].offset = offset;
    binds[nbinds].val = val;
    nbinds++;
}

static int bind_get(int offset, long *out)
{
    for (int i = nbinds - 1; i >= 0; i--) {
        if (binds[i].offset == offset) {
            *out = binds[i].val;
            return 1;
        }
    }
    return 0;
}

static int is_integer_const(Node *n, long *out)
{
    if (!n || n->kind != ND_NUM || !type_is_integer(n->ty))
        return 0;
    *out = n->val;
    return 1;
}

static int prop_expr(Node **np, int rvalue, int allow_subst);

static int prop_expr_rvalue(Node **np)
{
    return prop_expr(np, 1, 1);
}

static int prop_expr_list(Node **head, int allow_subst)
{
    int changed = 0;

    for (Node **np = head; *np; np = &(*np)->next)
        changed |= prop_expr(np, 1, allow_subst);
    return changed;
}

static void invalidate_all_binds(void)
{
    nbinds = 0;
}

static void invalidate_assigned_in_stmt(Node *s);

static void invalidate_assigned_in_list(Node *body)
{
    for (Node *s = body; s; s = s->next)
        invalidate_assigned_in_stmt(s);
}

static void invalidate_assigned_in_stmt(Node *s)
{
    if (!s)
        return;

    switch (s->kind) {
    case ND_DECL:
        if (type_is_integer(s->ty))
            bind_clear_offset(s->offset);
        return;
    case ND_EXPR_STMT:
        if (s->operand && s->operand->kind == ND_ASSIGN &&
            s->operand->lhs && s->operand->lhs->kind == ND_VAR &&
            type_is_integer(s->operand->lhs->ty))
            bind_clear_offset(s->operand->lhs->offset);
        return;
    case ND_BLOCK:
        invalidate_assigned_in_list(s->body);
        return;
    case ND_IF:
        invalidate_assigned_in_stmt(s->then_body);
        invalidate_assigned_in_stmt(s->else_body);
        return;
    case ND_WHILE:
    case ND_FOR:
        invalidate_assigned_in_stmt(s->then_body);
        return;
    default:
        return;
    }
}

static int prop_stmt_loop(Node *s);

static int prop_stmt_loop_list(Node *body)
{
    int changed = 0;

    for (Node *s = body; s; s = s->next)
        changed |= prop_stmt_loop(s);
    return changed;
}

static int prop_stmt_loop(Node *s)
{
    int changed = 0;

    if (!s)
        return 0;

    switch (s->kind) {
    case ND_DECL:
        changed |= prop_expr(&s->init, 1, 0);
        return changed;
    case ND_RETURN:
    case ND_EXPR_STMT:
        changed |= prop_expr(&s->operand, 1, 0);
        return changed;
    case ND_IF:
        changed |= prop_expr(&s->cond, 1, 0);
        changed |= prop_stmt_loop(s->then_body);
        changed |= prop_stmt_loop(s->else_body);
        return changed;
    case ND_BLOCK:
        bind_enter_scope();
        changed |= prop_stmt_loop_list(s->body);
        bind_leave_scope();
        return changed;
    case ND_WHILE:
    case ND_FOR:
        changed |= prop_expr(&s->cond, 1, 0);
        changed |= prop_stmt_loop(s->then_body);
        return changed;
    default:
        return 0;
    }
}

static int prop_expr(Node **np, int rvalue, int allow_subst)
{
    Node *n = *np;
    int changed = 0;
    long v;

    if (!n)
        return 0;

    changed |= cosfold_expr(np);
    n = *np;
    if (!n)
        return changed;

    if (rvalue && allow_subst && n->kind == ND_VAR &&
        type_is_integer(n->ty) && bind_get(n->offset, &v)) {
        *np = node_num(v, n->loc);
        (*np)->ty = n->ty;
        if (type_is_long(n->ty))
            (*np)->has_long_suffix = 1;
        return 1;
    }

    switch (n->kind) {
    case ND_BINOP:
        changed |= prop_expr(&n->lhs, 1, allow_subst);
        changed |= prop_expr(&n->rhs, 1, allow_subst);
        return changed;
    case ND_NEG:
    case ND_DEREF:
    case ND_CAST:
        changed |= prop_expr(&n->operand, 1, allow_subst);
        return changed;
    case ND_ADDR:
        changed |= prop_expr(&n->operand, 0, 0);
        if (n->operand && n->operand->kind == ND_VAR &&
            type_is_integer(n->operand->ty))
            bind_clear_offset(n->operand->offset);
        return changed;
    case ND_ASSIGN:
        if (n->lhs && n->lhs->kind == ND_DEREF)
            invalidate_all_binds();
        changed |= prop_expr(&n->rhs, 1, allow_subst);
        if (allow_subst && n->lhs && n->lhs->kind == ND_VAR &&
            type_is_integer(n->lhs->ty)) {
            if (is_integer_const(n->rhs, &v))
                bind_set(n->lhs->offset, v);
            else
                bind_clear_offset(n->lhs->offset);
        }
        return changed;
    case ND_CALL:
        return prop_expr_list(&n->args, 0);
    case ND_NUM:
    case ND_VAR:
        return changed;
    default:
        return changed;
    }
}

static int prop_stmt(Node *s);

static int prop_stmt_list(Node *body)
{
    int changed = 0;

    for (Node *s = body; s; s = s->next)
        changed |= prop_stmt(s);
    return changed;
}

static int prop_stmt(Node *s)
{
    int changed = 0;
    long v;

    if (!s)
        return 0;

    switch (s->kind) {
    case ND_DECL:
        changed |= prop_expr_rvalue(&s->init);
        if (type_is_integer(s->ty)) {
            if (is_integer_const(s->init, &v))
                bind_set(s->offset, v);
        }
        return changed;
    case ND_RETURN:
    case ND_EXPR_STMT:
        changed |= prop_expr_rvalue(&s->operand);
        return changed;
    case ND_IF: {
        BindSnap snap;

        bind_snapshot(&snap);
        changed |= prop_expr_rvalue(&s->cond);
        changed |= prop_stmt(s->then_body);
        bind_restore(&snap);
        changed |= prop_stmt(s->else_body);
        bind_restore(&snap);
        invalidate_assigned_in_stmt(s->then_body);
        invalidate_assigned_in_stmt(s->else_body);
        return changed;
    }
    case ND_WHILE:
        changed |= prop_expr(&s->cond, 1, 0);
        {
            BindSnap snap;

            bind_snapshot(&snap);
            bind_enter_scope();
            changed |= prop_stmt_loop(s->then_body);
            bind_leave_scope();
            bind_restore(&snap);
        }
        invalidate_assigned_in_stmt(s->then_body);
        return changed;
    case ND_FOR:
        changed |= prop_expr(&s->init, 1, 0);
        changed |= prop_expr(&s->cond, 1, 0);
        changed |= prop_expr(&s->step, 1, 0);
        {
            BindSnap snap;

            bind_snapshot(&snap);
            bind_enter_scope();
            changed |= prop_stmt_loop(s->then_body);
            bind_leave_scope();
            bind_restore(&snap);
        }
        invalidate_assigned_in_stmt(s->then_body);
        if (s->init && s->init->kind == ND_ASSIGN &&
            s->init->lhs && s->init->lhs->kind == ND_VAR &&
            type_is_integer(s->init->lhs->ty))
            bind_clear_offset(s->init->lhs->offset);
        return changed;
    case ND_BLOCK:
        changed |= prop_stmt_list(s->body);
        return changed;
    default:
        return 0;
    }
}

int cosprop_function(Function *fn)
{
    if (!fn || !fn->body)
        return 0;
    bind_clear();
    return prop_stmt_list(fn->body);
}
