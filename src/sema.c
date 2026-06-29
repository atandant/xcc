/* SPDX-License-Identifier: MIT */
#include "sema.h"
#include "diag.h"

#include <string.h>

#define MAX_LOCALS 1024
#define MAX_SCOPES 256
#define MAX_FUNCS  4096

/* ---- per-function local environment ---- */

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

/* return-statement context for the function being resolved */
static int cur_ret_void;
static const char *cur_fname;

/* frame-sizing accumulators (Option B: reserve temp + outgoing-arg areas) */
static int cur_max_depth; /* max simultaneously-live expression temps      */
static int cur_max_out;   /* max outgoing stack-arg bytes over all calls    */

/* ---- translation-unit function table ---- */

typedef struct {
    char *name;
    int prototyped;
    int nparams;
    int ret_void;
    int defined;
} FuncSym;

static FuncSym funcs[MAX_FUNCS];
static int nfuncs;

static FuncSym *find_func(const char *name)
{
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i].name, name) == 0)
            return &funcs[i];
    return NULL;
}

static FuncSym *add_func(char *name, int prototyped, int nparams,
                         int ret_void, int defined)
{
    FuncSym *s = &funcs[nfuncs++];
    s->name = name;
    s->prototyped = prototyped;
    s->nparams = nparams;
    s->ret_void = ret_void;
    s->defined = defined;
    return s;
}

/* Merge a top-level declaration/definition into the function table, emitting
 * redefinition / conflicting-type errors per C89. */
static void register_func(Function *fn)
{
    FuncSym *s = find_func(fn->name);
    if (!s) {
        add_func(fn->name, fn->prototyped, fn->nparams, fn->ret_void,
                 fn->is_definition);
        return;
    }

    if (fn->is_definition && s->defined)
        diag_error_at(fn->loc, "redefinition of '%s'", fn->name);
    else if (s->ret_void != fn->ret_void)
        diag_error_at(fn->loc, "conflicting types for '%s'", fn->name);
    else if (s->prototyped && fn->prototyped && s->nparams != fn->nparams)
        diag_error_at(fn->loc, "conflicting types for '%s'", fn->name);

    if (fn->prototyped && !s->prototyped) {
        s->prototyped = 1;
        s->nparams = fn->nparams;
    }
    if (fn->is_definition)
        s->defined = 1;
}

/* ---- scope helpers ---- */

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

/* Reserve one 8-byte slot below %rbp (int kept 64-bit, v0.0.1). */
static int alloc_slot(void)
{
    cur_offset -= 8;
    return cur_offset;
}

static void bind_local(char *name, int offset)
{
    locals[nlocals].name = name;
    locals[nlocals].offset = offset;
    nlocals++;
}

static int add_local(char *name)
{
    int off = alloc_slot();
    bind_local(name, off);
    return off;
}

/* ---- resolution ---- */

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
    case ND_CALL: {
        int off;
        if (lookup(n->name, &off)) {
            diag_error_at(n->loc, "called object '%s' is not a function",
                          n->name);
        } else {
            FuncSym *s = find_func(n->name);
            if (!s) {
                /* C89 implicit declaration: extern int name(); silent (D). */
                add_func(n->name, 0, 0, 0, 0);
            } else if (s->prototyped && s->nparams != n->nargs) {
                if (n->nargs < s->nparams)
                    diag_error_at(n->loc,
                                  "too few arguments to function '%s'",
                                  n->name);
                else
                    diag_error_at(n->loc,
                                  "too many arguments to function '%s'",
                                  n->name);
            }
        }
        for (Node *a = n->args; a; a = a->next)
            resolve_expr(a);
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
        if (s->operand) {
            if (cur_ret_void)
                diag_error_at(s->loc,
                              "void function '%s' should not return a value",
                              cur_fname);
            resolve_expr(s->operand);
        }
        /* bare `return;` is legal C89 in any function (UB only if used). */
        return;
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

/* ---- frame sizing (must be an upper bound on what codegen spills) ----
 *
 * Mirrors codegen's generic (non-fast-path) lowering. Fast paths only ever
 * spill fewer temps, so this is always a safe over-estimate. */

static int max_int(int a, int b) { return a > b ? a : b; }

static int is_imm(Node *n)
{
    return n->kind == ND_NUM;
}

static int is_local(Node *n)
{
    return n->kind == ND_VAR;
}

static int is_zero(Node *n)
{
    return is_imm(n) && n->val == 0;
}

static int is_cmp(BinOp op)
{
    return op == OP_EQ || op == OP_NE ||
           op == OP_LT || op == OP_LE ||
           op == OP_GT || op == OP_GE;
}

static int fits_i32(long val)
{
    return val >= -2147483647L - 1L && val <= 2147483647L;
}

static int is_direct_arg(Node *n)
{
    return is_imm(n) || is_local(n);
}

static int is_call_arg_direct(Node *n, int complex_after)
{
    return is_imm(n) || (is_local(n) && complex_after == 0);
}

static int measure_expr(Node *e)
{
    if (!e)
        return 0;

    switch (e->kind) {
    case ND_NUM:
    case ND_VAR:
        return 0;
    case ND_NEG:
        return measure_expr(e->operand);
    case ND_BINOP: {
        int r = measure_expr(e->rhs);
        int l = measure_expr(e->lhs);

        if (is_zero(e->rhs)) {
            switch (e->op) {
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
                return l;
            default:
                break;
            }
        }

        if (is_imm(e->rhs) && e->rhs->val == 1) {
            switch (e->op) {
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
                return l;
            default:
                break;
            }
        }

        if (is_imm(e->lhs) && e->lhs->val == 0 && e->op == OP_ADD)
            return r;

        if (is_imm(e->lhs) && e->lhs->val == 1 && e->op == OP_MUL)
            return r;

        if (is_cmp(e->op) && is_zero(e->rhs))
            return l;

        if (e->op != OP_DIV && e->op != OP_MOD) {
            if (is_imm(e->rhs) && fits_i32(e->rhs->val))
                return l;
            if (is_local(e->rhs))
                return l;
        }

        if (e->op == OP_ADD || e->op == OP_MUL ||
            e->op == OP_EQ || e->op == OP_NE) {
            if (is_imm(e->lhs) && fits_i32(e->lhs->val))
                return r;
            if (is_local(e->lhs))
                return r;
        }

        return max_int(r, 1 + l);   /* spill rhs, then evaluate lhs */
    }
    case ND_ASSIGN: {
        int r = measure_expr(e->rhs);
        int l = measure_expr(e->lhs);
        return max_int(l, 1 + r);   /* spill addr, then evaluate rhs */
    }
    case ND_CALL: {
        int peak = 0;
        int live = 0;
        int complex_after = 0;

        for (Node *a = e->args; a; a = a->next)
            if (!is_direct_arg(a))
                complex_after++;

        for (Node *a = e->args; a; a = a->next) {
            int direct = is_call_arg_direct(a, complex_after);

            peak = max_int(peak, live + measure_expr(a));
            if (!direct) {
                live++;
                peak = max_int(peak, live);
            }
            if (!is_direct_arg(a))
                complex_after--;
        }
        int stackargs = e->nargs > 6 ? e->nargs - 6 : 0;
        cur_max_out = max_int(cur_max_out, 8 * stackargs);
        return peak;
    }
    default:
        return 0;
    }
}

static void measure_stmt(Node *s);

static void measure_stmt_list(Node *body)
{
    for (Node *s = body; s; s = s->next)
        measure_stmt(s);
}

static void measure_stmt(Node *s)
{
    switch (s->kind) {
    case ND_DECL:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->init));
        return;
    case ND_RETURN:
    case ND_EXPR_STMT:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->operand));
        return;
    case ND_IF:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->cond));
        measure_stmt(s->then_body);
        if (s->else_body)
            measure_stmt(s->else_body);
        return;
    case ND_WHILE:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->cond));
        measure_stmt(s->then_body);
        return;
    case ND_FOR:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->init));
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->cond));
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->step));
        measure_stmt(s->then_body);
        return;
    case ND_BLOCK:
        measure_stmt_list(s->body);
        return;
    default:
        return;
    }
}

static void sema_function(Function *fn)
{
    nlocals = 0;
    nscopes = 0;
    cur_offset = 0;
    cur_max_depth = 0;
    cur_max_out = 0;
    cur_ret_void = fn->ret_void;
    cur_fname = fn->name;

    enter_scope();

    /* Parameters become the outermost locals. The first six live in argument
     * registers and get spilled into negative slots; the rest are passed on
     * the stack and referenced in place at positive offsets (16(%rbp)...). */
    int i = 0;
    for (Param *p = fn->params; p; p = p->next, i++) {
        if (i < 6)
            p->offset = alloc_slot();
        else
            p->offset = 16 + 8 * (i - 6);
        if (p->name) {
            if (declared_here(p->name))
                diag_error_at(fn->loc, "redefinition of parameter '%s'",
                              p->name);
            else
                bind_local(p->name, p->offset);
        }
    }

    resolve_stmt_list(fn->body);
    leave_scope();

    measure_stmt_list(fn->body);

    int locals_size = -cur_offset;
    int frame = locals_size + 8 * cur_max_depth + cur_max_out;
    fn->locals_size = locals_size;
    fn->stack_size = (frame + 15) & ~15;  /* 16-byte aligned frame */
}

void sema(Function *prog)
{
    nfuncs = 0;

    /* Single forward pass: each function's declaration becomes visible before
     * its body is resolved (so recursion works), and calls to not-yet-seen
     * names fall to C89 implicit declaration. */
    for (Function *fn = prog; fn; fn = fn->next) {
        register_func(fn);
        if (fn->is_definition)
            sema_function(fn);
    }
}
