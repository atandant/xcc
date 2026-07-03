/* SPDX-License-Identifier: MIT */
#include "sema.h"
#include "diag.h"
#include "intconst.h"

#include <limits.h>
#include <string.h>

#define MAX_LOCALS 1024
#define MAX_SCOPES 256
#define MAX_FUNCS  4096

/* ---- per-function local environment ---- */

typedef struct {
    char *name;
    Type *ty;
    int offset;
    SourceLoc loc;
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
static Type *cur_ret_ty;
static const char *cur_fname;

/* frame-sizing accumulators (Option B: reserve temp + outgoing-arg areas) */
static int cur_max_depth; /* max simultaneously-live expression temps      */
static int cur_max_out;   /* max outgoing stack-arg bytes over all calls    */

/* ---- translation-unit function table ---- */

typedef struct {
    char *name;
    Type *ty;
    int defined;
    int implicit;
    SourceLoc loc;
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

static FuncSym *add_func(char *name, Type *ty, int defined, int implicit,
                         SourceLoc loc)
{
    if (nfuncs >= MAX_FUNCS) {
        diag_error_at(loc, "too many functions in translation unit");
        return NULL;
    }
    FuncSym *s = &funcs[nfuncs++];
    s->name = name;
    s->ty = ty;
    s->defined = defined;
    s->implicit = implicit;
    s->loc = loc;
    return s;
}

/* Merge a top-level declaration/definition into the function table, emitting
 * redefinition / conflicting-type errors per C89.
 *
 * type_same on two TY_FUNC types yields the C89 merge rule we want: a bare
 * `()` declaration is compatible with a prototype (only the return type is
 * compared), while two prototypes must agree on parameter types and count. */
static void register_func(Function *fn)
{
    FuncSym *s = find_func(fn->name);
    if (!s) {
        if (!add_func(fn->name, fn->ty, fn->is_definition, 0, fn->loc))
            return;
        if (fn->is_definition && !fn->ty->prototyped)
            diag_warn(W_OLD_STYLE_FUNCTION_DEFINITION, fn->loc,
                      "function '%s' defined without a prototype", fn->name);
        return;
    }

    if (fn->is_definition && s->defined) {
        diag_error_at(fn->loc, "redefinition of '%s'", fn->name);
        diag_note_at(s->loc, "previous definition of '%s' is here", fn->name);
    } else if (!type_same(s->ty, fn->ty)) {
        diag_error_at(fn->loc, "conflicting types for '%s'", fn->name);
        diag_note_at(fn->loc, "conflicting declaration has type '%s'",
                     type_func_sig(fn->ty));
        diag_note_at(s->loc, "previous declaration is here with type '%s'",
                     type_func_sig(s->ty));
    }

    /* A later prototype refines an earlier unprototyped declaration. */
    if (fn->ty->prototyped && !s->ty->prototyped)
        s->ty = fn->ty;
    if (fn->is_definition) {
        s->defined = 1;
        s->loc = fn->loc;
    }
}

/* ---- scope helpers ---- */

static void enter_scope(void)
{
    if (nscopes >= MAX_SCOPES) {
        diag_error("too many nested scopes");
        return;
    }
    scopes[nscopes].start_local = nlocals;
    nscopes++;
}

static void leave_scope(void)
{
    if (nscopes <= 0)
        return;
    nscopes--;
    nlocals = scopes[nscopes].start_local;
}

static int lookup(const char *name, int *out_offset, Type **out_ty)
{
    for (int i = nlocals - 1; i >= 0; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            if (out_offset)
                *out_offset = locals[i].offset;
            if (out_ty)
                *out_ty = locals[i].ty;
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

/* Reserve stack space for a local object (type-aware size and alignment). */
static int align_down(int off, int align)
{
    unsigned u = (unsigned)(-off);
    unsigned rem = u % (unsigned)align;

    if (rem != 0)
        off -= (int)rem;
    return off;
}

static int alloc_local(Type *ty)
{
    int size = type_size(ty);
    int align = type_align(ty);

    if (size < 1) {
        diag_error("invalid object size for local variable");
        size = 1;
    }
    cur_offset -= size;
    if (align > 1)
        cur_offset = align_down(cur_offset, align);
    return cur_offset;
}

static int lookup_loc_here(const char *name, SourceLoc *out_loc)
{
    int start = scopes[nscopes - 1].start_local;

    for (int i = nlocals - 1; i >= start; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            *out_loc = locals[i].loc;
            return 1;
        }
    }
    return 0;
}

static void bind_local(char *name, Type *ty, int offset, SourceLoc loc)
{
    if (nlocals >= MAX_LOCALS) {
        diag_error_at(loc, "too many local variables in function");
        return;
    }
    locals[nlocals].name = name;
    locals[nlocals].ty = ty;
    locals[nlocals].offset = offset;
    locals[nlocals].loc = loc;
    nlocals++;
}

static int add_local(char *name, Type *ty, SourceLoc loc)
{
    int off = alloc_local(ty);
    bind_local(name, ty, off, loc);
    return off;
}

/* ---- resolution ---- */

static int is_arith_op(BinOp op)
{
    return op == OP_ADD || op == OP_SUB || op == OP_MUL ||
           op == OP_DIV || op == OP_MOD;
}

static int is_eq_op(BinOp op)
{
    return op == OP_EQ || op == OP_NE;
}

static int is_void_ptr_type(Type *ty)
{
    return type_is_pointer(ty) && type_is_void(ty->base);
}

/* C89 3.4 / 3.2.2.3: evaluate an integral constant expression (ICE). */
static void ice_apply_cast(Type *dst, long *val)
{
    if (!dst || !val)
        return;
    if (type_is_char(dst))
        *val &= 0xFF;
    else if (type_is_short(dst))
        *val = (long)(short)*val;
    else if (type_is_integer(dst) && type_int_width(dst) == 4)
        *val = (int)*val;
    /* long and wider targets keep the full signed value */
}

static int ice_eval(Node *n, long *out)
{
    long l, r;

    if (!n || !out)
        return 0;

    switch (n->kind) {
    case ND_NUM:
        if (!type_is_integer(n->ty))
            return 0;
        *out = n->val;
        return 1;
    case ND_NEG:
        if (!ice_eval(n->operand, out))
            return 0;
        return int_const_neg(*out, out);
    case ND_BINOP:
        if (!type_is_integer(n->ty))
            return 0;
        if (!ice_eval(n->lhs, &l) || !ice_eval(n->rhs, &r))
            return 0;
        return int_const_binop(n->op, l, r, out);
    case ND_CAST:
        if (!n->cast_ty || !type_is_integer(n->cast_ty))
            return 0;
        if (!ice_eval(n->operand, out))
            return 0;
        ice_apply_cast(n->cast_ty, out);
        return 1;
    default:
        return 0;
    }
}

/* C89 3.2.2.3: ICE with value 0, or such an expression cast to void *. */
static int is_null_ptr_constant(Node *n)
{
    long v;

    if (!n)
        return 0;
    if (ice_eval(n, &v) && v == 0)
        return 1;
    if (n->kind == ND_CAST && n->cast_ty && is_void_ptr_type(n->cast_ty))
        return ice_eval(n->operand, &v) && v == 0;
    return 0;
}

static void note_non_null_int_to_pointer(SourceLoc loc)
{
    diag_note_at(loc,
                 "only integer constant expression 0 is a null pointer constant");
}

static int is_null_ptr_eq(Node *lhs, Node *rhs)
{
    return (type_is_pointer(lhs->ty) && is_null_ptr_constant(rhs)) ||
           (type_is_pointer(rhs->ty) && is_null_ptr_constant(lhs));
}

/* Assignment-compatible, including null pointer constant 0 -> pointer. */
static int expr_assignable_to(Type *dst, Node *src)
{
    if (!dst || !src)
        return 0;
    if (type_assignable(dst, src->ty))
        return 1;
    return type_is_pointer(dst) && is_null_ptr_constant(src);
}

/* char storage is unsigned byte-wide (movzbl); warn on out-of-range constants. */
static void warn_value_conversion(SourceLoc loc, Type *dst, Node *src)
{
    if (!dst || !src || !src->ty)
        return;

    if (type_is_char(dst) && type_is_integer(src->ty) && !type_is_char(src->ty)) {
        if (src->kind == ND_NUM) {
            if (src->val < 0 || src->val > 255)
                diag_warn(W_INT_TO_CHAR_OVERFLOW, loc,
                          "overflow in conversion from '%s' to '%s'",
                          type_name(src->ty), type_name(dst));
        } else if (diag_warn_enabled(W_INT_TO_CHAR_CONVERSION)) {
            diag_warn(W_INT_TO_CHAR_CONVERSION, loc,
                      "conversion from '%s' to '%s' may alter value",
                      type_name(src->ty), type_name(dst));
        }
    }

    if (type_is_pointer(dst) && type_is_pointer(src->ty) &&
        type_assignable(dst, src->ty) && !type_same(dst, src->ty) &&
        (is_void_ptr_type(dst) || is_void_ptr_type(src->ty))) {
        diag_warn(W_POINTER_CONVERSION, loc,
                  "conversion from '%s' to '%s' without a cast",
                  type_name(src->ty), type_name(dst));
    }
}

static void check_init_from_self(Node *n, Node *decl)
{
    if (!n || !decl)
        return;

    if (n->kind == ND_VAR && strcmp(n->name, decl->name) == 0 &&
        n->offset == decl->offset) {
        diag_warn(W_INIT_FROM_SELF, n->loc,
                  "initializer refers to '%s' before its value is defined",
                  decl->name);
        diag_note_at(n->loc,
                     "'%s' has indeterminate value here; behavior is undefined in C89",
                     decl->name);
        return;
    }

    switch (n->kind) {
    case ND_BINOP:
        check_init_from_self(n->lhs, decl);
        check_init_from_self(n->rhs, decl);
        return;
    case ND_NEG:
    case ND_DEREF:
    case ND_CAST:
        check_init_from_self(n->operand, decl);
        return;
    case ND_ADDR:
        check_init_from_self(n->operand, decl);
        return;
    case ND_ASSIGN:
        check_init_from_self(n->lhs, decl);
        check_init_from_self(n->rhs, decl);
        return;
    case ND_CALL:
        for (Node *a = n->args; a; a = a->next)
            check_init_from_self(a, decl);
        return;
    default:
        return;
    }
}

static void diag_incompatible_assign(SourceLoc loc, Type *dst, Node *src)
{
    diag_error_at(loc,
                  "incompatible types assigning '%s' to '%s'",
                  type_name(src->ty), type_name(dst));
    if (type_is_pointer(dst) && type_is_integer(src->ty) &&
        !is_null_ptr_constant(src))
        note_non_null_int_to_pointer(loc);
}

static void diag_incompatible_init(SourceLoc loc, Type *dst, Node *src)
{
    diag_error_at(loc,
                  "incompatible types initializing '%s' with '%s'",
                  type_name(dst), type_name(src->ty));
    if (type_is_pointer(dst) && type_is_integer(src->ty) &&
        !is_null_ptr_constant(src))
        note_non_null_int_to_pointer(loc);
}

/* ---- lvalue helpers (sema is the single authority) ---- */

static int expr_is_lvalue(Node *n)
{
    return n && n->is_lvalue;
}

static int expr_is_modifiable_lvalue(Node *n)
{
    return expr_is_lvalue(n) && n->ty && type_is_object(n->ty);
}

/* Equality operands: both integers, compatible pointers, or pointer vs 0. */
static int eq_operands_compatible(Node *lhs, Node *rhs)
{
    return (type_is_integer(lhs->ty) && type_is_integer(rhs->ty)) ||
           (type_is_pointer(lhs->ty) && type_is_pointer(rhs->ty) &&
            type_compatible(lhs->ty, rhs->ty)) ||
           is_null_ptr_eq(lhs, rhs);
}

static int rel_operands_compatible(Node *lhs, Node *rhs)
{
    return (type_is_integer(lhs->ty) && type_is_integer(rhs->ty)) ||
           (type_is_pointer(lhs->ty) && type_is_pointer(rhs->ty) &&
            type_compatible(lhs->ty, rhs->ty));
}

typedef enum {
    CTX_RVALUE,
    CTX_LVALUE,
    CTX_ADDR_OPERAND
} ExprCtx;

static void resolve_expr_ctx(Node *n, ExprCtx ctx);

static int is_rel_op(BinOp op)
{
    return op == OP_LT || op == OP_LE || op == OP_GT || op == OP_GE;
}

static int check_arith_binop(Node *n)
{
    Type *l = n->lhs->ty;
    Type *r = n->rhs->ty;

    if (type_is_integer(l) && type_is_integer(r)) {
        n->ty = type_arith_convert(l, r);
        return 1;
    }

    if (n->op == OP_ADD) {
        /* C89 3.3.6: integer operand may be char, int, or long (after
         * integral promotion for narrower types). */
        if (type_is_pointer(l) && type_is_integer(r)) {
            n->ty = l;
            return 1;
        }
        if (type_is_integer(l) && type_is_pointer(r)) {
            n->ty = r;
            return 1;
        }
    }

    if (n->op == OP_SUB) {
        if (type_is_pointer(l) && type_is_integer(r)) {
            n->ty = l;
            return 1;
        }
        if (type_is_pointer(l) && type_is_pointer(r) &&
            type_compatible(l, r)) {
            n->ty = type_long();
            return 1;
        }
    }

    return 0;
}

static void resolve_expr_inner(Node *n, ExprCtx ctx);

/* Resolve names/offsets and assign a Type * (and lvalue flag) to every
 * expression node, then apply array-to-pointer decay. C89 3.2.2.1: an array
 * value used in an rvalue context is converted to a pointer to its first
 * element. This is a general conversion, not a variable-only one, so it also
 * fires for the intermediate array produced by `*(a + i)` when indexing a
 * multi-dimensional array. A decayed array's value is its address, which
 * codegen emits via gen_addr (keyed off var_decay). */
static void resolve_expr_ctx(Node *n, ExprCtx ctx)
{
    if (!n)
        return;

    resolve_expr_inner(n, ctx);

    if (ctx == CTX_RVALUE && type_is_array(n->ty)) {
        n->ty = type_decay(n->ty);
        n->is_lvalue = 0;
        n->var_decay = 1;
    }
}

/* On any error we still set a fallback type so later checks don't dereference
 * a NULL Type. */
static void resolve_expr_inner(Node *n, ExprCtx ctx)
{
    if (!n)
        return;

    switch (n->kind) {
    case ND_NUM:
        /* C89 3.1.5: an L/l suffix forces long; an unsuffixed decimal is int
         * if it fits, otherwise long. (Overflow past signed long is caught by
         * the lexer.) */
        if (n->has_long_suffix)
            n->ty = type_long();
        else if (n->val >= -2147483647L - 1L && n->val <= 2147483647L)
            n->ty = type_int();
        else
            n->ty = type_long();
        n->is_lvalue = 0;
        n->var_decay = 0;
        return;
    case ND_VAR: {
        int off;
        Type *decl_ty;

        n->var_decay = 0;
        if (!lookup(n->name, &off, &decl_ty)) {
            diag_error_at(n->loc, "use of undeclared identifier '%s'", n->name);
            n->ty = type_int();
            n->is_lvalue = 0;
        } else {
            n->offset = off;
            n->ty = decl_ty;
            /* Array decay (rvalue context) is applied by resolve_expr_ctx. */
            n->is_lvalue = (ctx != CTX_RVALUE) && type_is_object(decl_ty);
        }
        return;
    }
    case ND_CALL: {
        int off;
        FuncSym *s = NULL;

        n->ty = type_int();
        n->func_ty = NULL;
        n->is_lvalue = 0;
        n->var_decay = 0;
        if (n->nargs > XCC_MAX_CALL_ARGS) {
            diag_error_at(n->loc, "too many arguments in function call");
        }
        for (Node *a = n->args; a; a = a->next)
            resolve_expr_ctx(a, CTX_RVALUE);

        if (lookup(n->name, &off, NULL)) {
            diag_error_at(n->loc, "called object '%s' is not a function",
                          n->name);
        } else {
            s = find_func(n->name);
            if (!s) {
                s = add_func(n->name, type_func(type_int(), NULL, 0, 0), 0, 1,
                             n->loc);
                if (s)
                    diag_warn(W_IMPLICIT_FUNCTION_DECLARATION, n->loc,
                              "implicit declaration of function '%s'", n->name);
            } else {
                if (s->implicit)
                    diag_warn(W_IMPLICIT_FUNCTION_DECLARATION, n->loc,
                              "implicit declaration of function '%s'",
                              n->name);
                if (!s->ty->prototyped && !s->implicit)
                    diag_warn(W_UNPROTOTYPED_FUNCTION_CALL, n->loc,
                              "call to function '%s' without a prototype",
                              n->name);
                if (s->ty->prototyped && s->ty->nparams != n->nargs) {
                    if (n->nargs < s->ty->nparams)
                        diag_error_at(n->loc,
                                      "too few arguments to function '%s'",
                                      n->name);
                    else
                        diag_error_at(n->loc,
                                      "too many arguments to function '%s'",
                                      n->name);
                } else if (s->ty->prototyped) {
                    int i = 0;
                    for (Node *a = n->args; a; a = a->next, i++) {
                        if (!expr_assignable_to(s->ty->params[i], a))
                            diag_error_at(a->loc,
                                          "passing '%s' to parameter of type '%s' in call to '%s'",
                                          type_name(a->ty),
                                          type_name(s->ty->params[i]),
                                          n->name);
                        else
                            warn_value_conversion(a->loc, s->ty->params[i], a);
                    }
                }
            }
            if (s) {
                n->func_ty = s->ty;
                n->ty = s->ty->ret;
            }
        }
        return;
    }
    case ND_ASSIGN:
        resolve_expr_ctx(n->lhs, CTX_LVALUE);
        resolve_expr_ctx(n->rhs, CTX_RVALUE);
        n->var_decay = 0;
        if (!expr_is_modifiable_lvalue(n->lhs))
            diag_error_at(n->loc, "assignment to non-lvalue");
        else if (!expr_assignable_to(n->lhs->ty, n->rhs))
            diag_incompatible_assign(n->loc, n->lhs->ty, n->rhs);
        else
            warn_value_conversion(n->loc, n->lhs->ty, n->rhs);
        n->ty = n->lhs->ty;
        n->is_lvalue = 0;
        return;
    case ND_BINOP:
        resolve_expr_ctx(n->lhs, CTX_RVALUE);
        resolve_expr_ctx(n->rhs, CTX_RVALUE);
        n->var_decay = 0;
        if (is_arith_op(n->op)) {
            if (!check_arith_binop(n))
                diag_error_at(n->loc,
                              "invalid operands to arithmetic operator");
        } else if (is_eq_op(n->op)) {
            if (!eq_operands_compatible(n->lhs, n->rhs)) {
                if (type_is_pointer(n->lhs->ty) && type_is_pointer(n->rhs->ty))
                    diag_error_at(n->loc,
                                  "comparison between incompatible pointer types '%s' and '%s'",
                                  type_name(n->lhs->ty),
                                  type_name(n->rhs->ty));
                else if (type_is_pointer(n->lhs->ty) &&
                         type_is_integer(n->rhs->ty))
                    diag_error_at(n->loc,
                                  "comparison between pointer type '%s' and integer type '%s'",
                                  type_name(n->lhs->ty),
                                  type_name(n->rhs->ty));
                else if (type_is_integer(n->lhs->ty) &&
                         type_is_pointer(n->rhs->ty))
                    diag_error_at(n->loc,
                                  "comparison between integer type '%s' and pointer type '%s'",
                                  type_name(n->lhs->ty),
                                  type_name(n->rhs->ty));
                else
                    diag_error_at(n->loc, "invalid operands to comparison");
            }
            n->ty = type_int();
        } else if (is_rel_op(n->op)) {
            if (!rel_operands_compatible(n->lhs, n->rhs))
                diag_error_at(n->loc,
                              "invalid operands to relational operator");
            n->ty = type_int();
        }
        n->is_lvalue = 0;
        return;
    case ND_NEG:
        resolve_expr_ctx(n->operand, CTX_RVALUE);
        n->var_decay = 0;
        if (!type_is_integer(n->operand->ty))
            diag_error_at(n->loc, "invalid operand to unary minus");
        n->ty = type_int_promote(n->operand->ty);
        n->is_lvalue = 0;
        return;
    case ND_ADDR:
        resolve_expr_ctx(n->operand, CTX_ADDR_OPERAND);
        n->var_decay = 0;
        if (!expr_is_lvalue(n->operand))
            diag_error_at(n->loc, "cannot take address of non-lvalue");
        n->ty = type_ptr(n->operand->ty);
        n->is_lvalue = 0;
        return;
    case ND_DEREF:
        resolve_expr_ctx(n->operand, CTX_RVALUE);
        n->var_decay = 0;
        if (!type_is_pointer(n->operand->ty)) {
            diag_error_at(n->loc, "cannot dereference non-pointer type '%s'",
                          type_name(n->operand->ty));
            n->ty = type_int();
        } else if (type_is_void(n->operand->ty->base)) {
            diag_error_at(n->loc, "dereference of void pointer");
            diag_note_at(n->loc,
                         "xcc supports void * conversions but not void * dereference");
            n->ty = type_int();
        } else {
            n->ty = n->operand->ty->base;
            n->is_lvalue = 1;
        }
        return;
    case ND_CAST: {
        Type *dst = n->cast_ty;

        resolve_expr_ctx(n->operand, CTX_RVALUE);
        n->var_decay = 0;
        n->is_lvalue = 0;

        /* C89 3.3.4: the target must be void or scalar, and (unless the
         * target is void) the operand must also have scalar type. An explicit
         * cast is the programmer's escape hatch, so it silences the implicit
         * conversion warnings that assignment would raise. */
        if (type_is_void(dst)) {
            /* (void)e discards the value; any operand type is fine. */
        } else if (!type_is_scalar(dst)) {
            diag_error_at(n->loc, "cast to non-scalar type '%s'",
                          type_name(dst));
        } else if (!type_is_scalar(n->operand->ty)) {
            diag_error_at(n->loc,
                          "operand of cast has non-scalar type '%s'",
                          type_name(n->operand->ty));
        }

        n->ty = dst;
        return;
    }
    default:
        return;
    }
}

static void resolve_stmt(Node *s);

/* A controlling expression (if/while/for condition) must be scalar: an
 * integer or a pointer. A NULL `for` condition is an infinite loop. */
static void require_scalar_cond(Node *cond)
{
    if (cond && !type_is_scalar(cond->ty))
        diag_error_at(cond->loc, "condition has non-scalar type '%s'",
                      type_name(cond->ty));
}

static void resolve_stmt_list(Node *body)
{
    for (Node *s = body; s; s = s->next)
        resolve_stmt(s);
}

static void resolve_stmt(Node *s)
{
    switch (s->kind) {
    case ND_DECL:
        if (!type_is_object(s->ty) || type_is_void(s->ty) ||
            (type_is_array(s->ty) && type_is_void(type_array_elem(s->ty))))
            diag_error_at(s->loc,
                          "variable '%s' has non-object type '%s'",
                          s->name, type_name(s->ty));
        if (type_is_array(s->ty) && type_array_count(s->ty) == 0)
            diag_error_at(s->loc,
                          "array size is missing or zero for '%s'",
                          s->name);
        if (declared_here(s->name)) {
            SourceLoc prev;
            lookup_loc_here(s->name, &prev);
            diag_error_at(s->loc, "redeclaration of '%s'", s->name);
            diag_note_at(prev, "previous declaration of '%s' is here", s->name);
            lookup(s->name, &s->offset, NULL);
        } else {
            s->offset = add_local(s->name, s->ty, s->loc);
        }
        /* C89 3.1.2.1: the name is in scope within its own initializer. */
        resolve_expr_ctx(s->init, CTX_RVALUE);
        if (s->init)
            check_init_from_self(s->init, s);
        if (s->init && !expr_assignable_to(s->ty, s->init))
            diag_incompatible_init(s->loc, s->ty, s->init);
        else if (s->init)
            warn_value_conversion(s->loc, s->ty, s->init);
        return;
    case ND_RETURN:
        if (s->operand) {
            if (type_is_void(cur_ret_ty))
                diag_error_at(s->loc,
                              "void function '%s' should not return a value",
                              cur_fname);
            resolve_expr_ctx(s->operand, CTX_RVALUE);
            if (!type_is_void(cur_ret_ty) &&
                !expr_assignable_to(cur_ret_ty, s->operand)) {
                diag_error_at(s->loc,
                              "returning '%s' from a function returning '%s'",
                              type_name(s->operand->ty), type_name(cur_ret_ty));
                if (type_is_pointer(cur_ret_ty) && type_is_integer(s->operand->ty) &&
                    !is_null_ptr_constant(s->operand))
                    note_non_null_int_to_pointer(s->loc);
            } else if (!type_is_void(cur_ret_ty))
                warn_value_conversion(s->loc, cur_ret_ty, s->operand);
        } else if (!type_is_void(cur_ret_ty)) {
            diag_warn(W_RETURN_TYPE, s->loc,
                      "non-void function '%s' should return a value",
                      cur_fname);
        }
        return;
    case ND_EXPR_STMT:
        resolve_expr_ctx(s->operand, CTX_RVALUE);
        return;
    case ND_IF:
        resolve_expr_ctx(s->cond, CTX_RVALUE);
        require_scalar_cond(s->cond);
        resolve_stmt(s->then_body);
        if (s->else_body)
            resolve_stmt(s->else_body);
        return;
    case ND_WHILE:
        resolve_expr_ctx(s->cond, CTX_RVALUE);
        require_scalar_cond(s->cond);
        resolve_stmt(s->then_body);
        return;
    case ND_FOR:
        resolve_expr_ctx(s->init, CTX_RVALUE);
        resolve_expr_ctx(s->cond, CTX_RVALUE);
        require_scalar_cond(s->cond);
        resolve_expr_ctx(s->step, CTX_RVALUE);
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

/* Mirror codegen binop_width: 8 for pointer/long operands, else 4. */
static int measure_binop_width(Node *lhs, Node *rhs)
{
    if (!lhs || !rhs)
        return 4;
    if (type_is_pointer(lhs->ty) || type_is_pointer(rhs->ty))
        return 8;
    if (type_is_long(lhs->ty) || type_is_long(rhs->ty))
        return 8;
    return 4;
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
    case ND_ADDR:
    case ND_DEREF:
    case ND_CAST:
        return measure_expr(e->operand);
    case ND_BINOP: {
        int r = measure_expr(e->rhs);
        int l = measure_expr(e->lhs);
        int w = measure_binop_width(e->lhs, e->rhs);

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

        /* 32-bit fast paths avoid a rhs spill; 64-bit lowering always spills. */
        if (w == 4 && e->op != OP_DIV && e->op != OP_MOD) {
            if (is_imm(e->rhs) && fits_i32(e->rhs->val))
                return l;
            if (is_local(e->rhs))
                return l;
        }

        if (w == 4 && (e->op == OP_ADD || e->op == OP_MUL ||
                       e->op == OP_EQ || e->op == OP_NE)) {
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
    cur_ret_ty = fn->ret_ty;
    cur_fname = fn->name;

    enter_scope();

    /* Parameters become the outermost locals. The first six live in argument
     * registers and get spilled into negative slots; the rest are passed on
     * the stack and referenced in place at positive offsets (16(%rbp)...). */
    int i = 0;
    for (Param *p = fn->params; p; p = p->next, i++) {
        Type *pty = type_decay(p->ty);

        if (i < 6)
            p->offset = alloc_local(pty);
        else
            p->offset = 16 + 8 * (i - 6);
        if (p->name) {
            if (declared_here(p->name))
                diag_error_at(fn->loc, "redefinition of parameter '%s'",
                              p->name);
            else
                bind_local(p->name, pty, p->offset, fn->loc);
        }
    }

    resolve_stmt_list(fn->body);
    leave_scope();

    measure_stmt_list(fn->body);

    int locals_size = -cur_offset;
    long frame = (long)locals_size + 8L * cur_max_depth + cur_max_out;

    if (frame < 0 || frame > INT_MAX - 15) {
        diag_error_at(fn->loc, "stack frame for '%s' is too large", fn->name);
        frame = locals_size;
    }
    fn->locals_size = locals_size;
    fn->stack_size = (int)((frame + 15) & ~15);  /* 16-byte aligned frame */
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
