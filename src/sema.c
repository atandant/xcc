/* SPDX-License-Identifier: MIT */
#include "sema.h"
#include "diag.h"

#include <string.h>

#define MAX_LOCALS 1024
#define MAX_SCOPES 256

typedef struct {
    char *name;
    int offset;
} Local;

typedef struct {
    int start_local;
} Scope;

static Local locals[MAX_LOCALS];
static Scope scopes[MAX_SCOPES];
static int nlocals;
static int nscopes;
static int cur_offset; /* grows downward from %rbp, in bytes */

static void enter_scope(void)
{
    scopes[nscopes].start_local = nlocals;
    nscopes++;
}

static void leave_scope(void)
{
    nscopes--;
    nlocals = scopes[nscopes].start_local;
}

static int lookup(const char *name, int *out_offset)
{
    for (int i = nlocals - 1; i >= 0; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            *out_offset = locals[i].offset;
            return 1;
        }
    }
    return 0;
}

static int declared_here(const char *name)
{
    int start = scopes[nscopes - 1].start_local;
    for (int i = start; i < nlocals; i++)
        if (strcmp(locals[i].name, name) == 0)
            return 1;
    return 0;
}

static int add_local(char *name)
{
    cur_offset -= 8; /* one 8-byte slot per local (int kept 64-bit, v0.0.1) */
    locals[nlocals].name = name;
    locals[nlocals].offset = cur_offset;
    nlocals++;
    return cur_offset;
}

static void resolve_expr(Node *n)
{
    if (!n)
        return;

    switch (n->kind) {
    case ND_NUM:
        return;
    case ND_VAR: {
        int off;
        if (!lookup(n->name, &off))
            diag_error_at(n->loc, "use of undeclared identifier '%s'", n->name);
        else
            n->offset = off;
        return;
    }
    case ND_ASSIGN:
        if (n->lhs->kind != ND_VAR)
            diag_error_at(n->loc, "assignment to non-lvalue");
        resolve_expr(n->lhs);
        resolve_expr(n->rhs);
        return;
    case ND_BINOP:
        resolve_expr(n->lhs);
        resolve_expr(n->rhs);
        return;
    case ND_NEG:
        resolve_expr(n->operand);
        return;
    default:
        return;
    }
}

static void resolve_stmt(Node *s);

static void resolve_stmt_list(Node *body)
{
    for (Node *s = body; s; s = s->next)
        resolve_stmt(s);
}

static void resolve_stmt(Node *s)
{
    switch (s->kind) {
    case ND_DECL:
        if (declared_here(s->name)) {
            diag_error_at(s->loc, "redeclaration of '%s'", s->name);
            lookup(s->name, &s->offset);
        } else {
            s->offset = add_local(s->name);
        }
        /* The name is in scope within its own initializer (C semantics). */
        resolve_expr(s->init);
        return;
    case ND_RETURN:
    case ND_EXPR_STMT:
        resolve_expr(s->operand);
        return;
    case ND_IF:
        resolve_expr(s->cond);
        resolve_stmt(s->then_body);
        if (s->else_body)
            resolve_stmt(s->else_body);
        return;
    case ND_WHILE:
        resolve_expr(s->cond);
        resolve_stmt(s->then_body);
        return;
    case ND_FOR:
        resolve_expr(s->init);
        resolve_expr(s->cond);
        resolve_expr(s->step);
        resolve_stmt(s->then_body);
        return;
    case ND_BLOCK:
        enter_scope();
        resolve_stmt_list(s->body);
        leave_scope();
        return;
    default:
        return;
    }
}

void sema(Function *fn)
{
    nlocals = 0;
    nscopes = 0;
    cur_offset = 0;

    enter_scope();
    resolve_stmt_list(fn->body);
    leave_scope();

    int size = -cur_offset;              /* make positive */
    fn->stack_size = (size + 15) & ~15;  /* 16-byte aligned frame */
}
