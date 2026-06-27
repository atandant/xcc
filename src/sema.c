#include "sema.h"
#include "diag.h"

#include <string.h>

/* v0.0.1 has a single, function-wide scope (no nested blocks yet). */
#define MAX_LOCALS 1024

typedef struct {
    char *name;
    int offset;
} Local;

static Local locals[MAX_LOCALS];
static int nlocals;
static int cur_offset; /* grows downward from %rbp, in bytes */

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
    for (int i = 0; i < nlocals; i++)
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

void sema(Function *fn)
{
    nlocals = 0;
    cur_offset = 0;

    for (Node *s = fn->body; s; s = s->next) {
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
            break;
        case ND_RETURN:
        case ND_EXPR_STMT:
            resolve_expr(s->operand);
            break;
        default:
            break;
        }
    }

    int size = -cur_offset;              /* make positive */
    fn->stack_size = (size + 15) & ~15;  /* 16-byte aligned frame */
}
