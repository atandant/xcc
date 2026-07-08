/* SPDX-License-Identifier: MIT */
#include "sema.h"
#include "sema_scope.h"
#include "sema_functab.h"
#include "sema_typedef.h"
#include "sema_struct.h"
#include "diag.h"
#include "intconst.h"

#include <limits.h>
#include <string.h>

/* UNDEFER: -Wnarrowing for unsigned-to-signed assignment with out-of-range constants. */
/* UNDEFER: reject comparisons between pointers and unsigned integers (stricter C89). */

/* return-statement context for the function being resolved */
static Type *cur_ret_ty;
static const char *cur_fname;

/* ---- resolution ---- */

typedef enum {
    CTX_RVALUE,
    CTX_LVALUE,
    CTX_ADDR_OPERAND,
    CTX_SIZEOF_OPERAND
} ExprCtx;

static void resolve_expr_ctx(Node *n, ExprCtx ctx);
static long sizeof_value(Type *ty, SourceLoc loc);

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
static int ice_eval(Node *n, long *out)
{
    long l, r;

    if (!n || !out)
        return 0;

    switch (n->kind) {
    case ND_NUM:
        if (n->ty && !type_is_integer(n->ty))
            return 0;
        *out = n->val;
        return 1;
    case ND_SIZEOF: {
        Type *ty = n->cast_ty;

        if (n->operand) {
            resolve_expr_ctx(n->operand, CTX_SIZEOF_OPERAND);
            ty = n->operand->ty;
        }
        if (!ty)
            return 0;
        *out = sizeof_value(ty, n->loc);
        return 1;
    }
    case ND_NEG:
        if (!ice_eval(n->operand, out))
            return 0;
        return int_const_neg_ty(*out, n->ty ? n->ty : type_long(), out);
    case ND_BINOP:
        if (n->ty && !type_is_integer(n->ty))
            return 0;
        if (!n->ty && !is_arith_op(n->op))
            return 0;
        if (!ice_eval(n->lhs, &l) || !ice_eval(n->rhs, &r))
            return 0;
        {
            Type *bty = n->ty ? n->ty : type_long();
            if (is_eq_op(n->op) ||
                n->op == OP_LT || n->op == OP_LE ||
                n->op == OP_GT || n->op == OP_GE)
                bty = type_arith_convert(n->lhs->ty, n->rhs->ty);
            return int_const_binop_ty(n->op, l, r, bty, out);
        }
    case ND_CAST:
        if (!n->cast_ty || !type_is_integer(n->cast_ty))
            return 0;
        if (!ice_eval(n->operand, out))
            return 0;
        *out = type_convert_const(*out, n->cast_ty);
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

    if (type_is_plain_char(dst) && type_is_integer(src->ty) && !type_is_char(src->ty)) {
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
    case ND_MEMBER:
        check_init_from_self(n->lhs, decl);
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
    case ND_INIT_LIST:
        for (Node *e = n->body; e; e = e->next)
            check_init_from_self(e, decl);
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

/* ND_MEMBER / ND_ADDR(&member): mark the enclosing local when its address escapes. */
static void mark_member_base_address_taken(Node *n)
{
    while (n) {
        if (n->kind == ND_VAR) {
            scope_mark_address_taken(n->name);
            return;
        }
        if (n->kind == ND_DEREF) {
            n = n->operand;
            continue;
        }
        if (n->kind == ND_MEMBER) {
            n = n->lhs;
            continue;
        }
        return;
    }
}

static Type *member_owner_struct(Node *base)
{
    if (!base || !base->ty)
        return NULL;
    if (type_is_struct(base->ty))
        return base->ty;
    return NULL;
}

static int struct_cmp_error(Node *lhs, Node *rhs)
{
    return type_is_struct(lhs->ty) || type_is_struct(rhs->ty);
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

static int sema_array_bound_eval(Node *expr, long *out, SourceLoc loc, void *ctx)
{
    (void)ctx;

    if (!expr) {
        *out = 0;
        return 1;
    }
    if (!ice_eval(expr, out)) {
        diag_error_at(loc, "array size is not an integer constant expression");
        return 0;
    }
    return 1;
}

static int type_is_aggregate(Type *t)
{
    return type_is_array(t) || type_is_struct(t);
}

/* Number of scalar leaf slots contained in an aggregate type tree. */
static int type_init_slot_count(Type *t)
{
    if (type_is_array(t))
        return type_array_count(t) * type_init_slot_count(type_array_elem(t));
    if (type_is_struct(t)) {
        int n = 0;
        int i;

        for (i = 0; i < t->nmembers; i++)
            n += type_init_slot_count(t->members[i].ty);
        return n;
    }
    return 1;
}

/* Count how many outermost-dimension elements a brace initializer fills,
   mirroring init_aggregate's element boundaries: a nested `{...}` fills exactly
   one element; a run of scalars fills up to `per` leaves per element (a `{` ends
   the current run and starts the next element). Used only for unsized `T a[]`. */
static int infer_unsized_count(Type *elem, Node *body)
{
    int per = type_init_slot_count(elem);
    int outer = 0;
    Node *e = body;

    if (per <= 0)
        return 0;
    while (e) {
        outer++;
        if (e->kind == ND_INIT_LIST) {
            e = e->next;
        } else {
            int k = 0;
            while (e && e->kind != ND_INIT_LIST && k < per) {
                e = e->next;
                k++;
            }
        }
    }
    return outer;
}

/* Patch an unsized outermost array bound `T a[] = {...}` in place from its
   brace initializer. The declarator type is arena-owned and unshared. */
static void infer_unsized_array(Node *decl)
{
    Type *elem = type_array_elem(decl->ty);
    int count;

    if (elem && type_is_array(elem) && type_array_count(elem) == 0) {
        diag_error_at(decl->loc,
                      "cannot infer size of '%s': inner array dimension is "
                      "also unsized", decl->name);
        return;
    }
    count = infer_unsized_count(elem, decl->init->body);
    if (count <= 0)
        return;          /* empty init -> existing zero-size diagnostic fires */
    decl->ty->count = count;
    decl->ty->size = type_size(elem) * count;
}

typedef struct {
    Node *head;
    Node **tail;
} InitFlat;

static void flat_append(InitFlat *f, Node *e)
{
    e->next = NULL;
    *f->tail = e;
    f->tail = &e->next;
}

static Node *zero_init_expr(Type *leaf, SourceLoc loc)
{
    Node *z = node_num(0, loc);
    z->ty = leaf;
    return z;
}

static void pad_aggregate_zeros(Type *t, InitFlat *flat, SourceLoc loc)
{
    if (type_is_array(t)) {
        Type *elem = type_array_elem(t);
        int n = type_array_count(t);
        int i;

        for (i = 0; i < n; i++)
            pad_aggregate_zeros(elem, flat, loc);
        return;
    }
    if (type_is_struct(t)) {
        int i;

        for (i = 0; i < t->nmembers; i++)
            pad_aggregate_zeros(t->members[i].ty, flat, loc);
        return;
    }
    flat_append(flat, zero_init_expr(t, loc));
}

static int init_scalar_brace_unwrap(Node **pcursor, InitFlat *flat, SourceLoc loc)
{
    Node *sub = (*pcursor)->body;
    Node *inner = sub;

    if (!inner || inner->kind == ND_INIT_LIST) {
        diag_error_at(loc, "too many braces around scalar initializer");
        return 1;
    }
    if (inner->next != NULL) {
        diag_error_at(loc, "excess elements in scalar initializer");
        return 1;
    }
    *pcursor = (*pcursor)->next;
    inner->next = NULL;
    flat_append(flat, inner);
    return 0;
}

static int init_aggregate(Type *t, Node **pcursor, InitFlat *flat,
                          SourceLoc loc, int *excess);

static int init_aggregate(Type *t, Node **pcursor, InitFlat *flat,
                          SourceLoc loc, int *excess)
{
    if (type_is_array(t)) {
        Type *elem = type_array_elem(t);
        int n = type_array_count(t);
        int i;

        for (i = 0; i < n; i++) {
            if (!*pcursor) {
                pad_aggregate_zeros(elem, flat, loc);
                continue;
            }
            if ((*pcursor)->kind == ND_INIT_LIST) {
                Node *sub = (*pcursor)->body;
                Node **subcur = &sub;

                *pcursor = (*pcursor)->next;
                if (init_aggregate(elem, subcur, flat, loc, excess))
                    return 1;
                if (*subcur != NULL)
                    *excess = 1;
            } else if (init_aggregate(elem, pcursor, flat, loc, excess)) {
                return 1;
            }
        }
        return 0;
    }

    if (type_is_struct(t)) {
        int i;

        if (!type_struct_is_complete(t)) {
            diag_error_at(loc,
                          "initializer element is not computable for "
                          "incomplete type '%s'", type_name(t));
            return 1;
        }
        for (i = 0; i < t->nmembers; i++) {
            Type *mty = t->members[i].ty;

            if (!*pcursor) {
                pad_aggregate_zeros(mty, flat, loc);
                continue;
            }
            if ((*pcursor)->kind == ND_INIT_LIST) {
                if (type_is_aggregate(mty)) {
                    Node *sub = (*pcursor)->body;
                    Node **subcur = &sub;

                    *pcursor = (*pcursor)->next;
                    if (init_aggregate(mty, subcur, flat, loc, excess))
                        return 1;
                    if (*subcur != NULL)
                        *excess = 1;
                } else if (init_scalar_brace_unwrap(pcursor, flat, loc)) {
                    return 1;
                }
            } else if (init_aggregate(mty, pcursor, flat, loc, excess)) {
                return 1;
            }
        }
        return 0;
    }

    if (!*pcursor) {
        flat_append(flat, zero_init_expr(t, loc));
        return 0;
    }
    if ((*pcursor)->kind == ND_INIT_LIST) {
        diag_error_at(loc, "brace-enclosed initializer for scalar element");
        return 1;
    }
    {
        Node *e = *pcursor;

        *pcursor = e->next;
        e->next = NULL;
        flat_append(flat, e);
    }
    return 0;
}

static Node *flatten_brace_init(Type *aty, Node *init, SourceLoc loc)
{
    InitFlat flat = { NULL, &flat.head };
    Node *cursor = init->body;
    int excess = 0;

    if (init_aggregate(aty, &cursor, &flat, loc, &excess))
        return init;
    if (cursor != NULL || excess) {
        if (type_is_struct(aty))
            diag_error_at(loc, "excess elements in struct initializer");
        else
            diag_error_at(loc, "excess elements in array initializer");
    }
    return node_init_list(flat.head, init->loc);
}

static void resolve_init_expr(Node *n, ExprCtx ctx);

static void resolve_init_expr(Node *n, ExprCtx ctx)
{
    if (!n)
        return;
    if (n->kind == ND_INIT_LIST) {
        for (Node *e = n->body; e; e = e->next)
            resolve_init_expr(e, ctx);
        return;
    }
    resolve_expr_ctx(n, ctx);
}

static void resolve_init_list(Node *init, ExprCtx ctx)
{
    resolve_init_expr(init, ctx);
}

static void check_init_list_from_self(Node *init, Node *decl)
{
    if (!init || init->kind != ND_INIT_LIST)
        return;
    for (Node *e = init->body; e; e = e->next)
        check_init_from_self(e, decl);
}

static Node *check_flat_init_type(Type *t, Node *e, SourceLoc loc, Node *decl)
{
    int i;

    if (type_is_array(t)) {
        Type *elem = type_array_elem(t);
        int n = type_array_count(t);

        for (i = 0; i < n; i++)
            e = check_flat_init_type(elem, e, loc, decl);
        return e;
    }
    if (type_is_struct(t)) {
        for (i = 0; i < t->nmembers; i++)
            e = check_flat_init_type(t->members[i].ty, e, loc, decl);
        return e;
    }
    if (!e)
        return NULL;
    if (!expr_assignable_to(t, e))
        diag_incompatible_init(e->loc, t, e);
    else
        warn_value_conversion(e->loc, t, e);
    return e->next;
}

/* C89 3.5.7: a scalar may be initialized by a brace-enclosed single
   expression, e.g. `int x = {3};` (optionally with a trailing comma). Unwrap it
   to an ordinary scalar initializer so lowering emits a plain store. */
static void sema_scalar_brace_init(Node *decl)
{
    Node *body = decl->init->body;
    Node *val;

    if (!body) {
        diag_error_at(decl->loc, "empty scalar initializer");
        return;
    }
    if (body->kind == ND_INIT_LIST) {
        diag_error_at(decl->loc, "too many braces around scalar initializer");
        return;
    }
    if (body->next != NULL) {
        diag_error_at(decl->loc, "excess elements in scalar initializer");
        return;
    }

    val = body;
    val->next = NULL;
    decl->init = val;

    resolve_expr_ctx(val, CTX_RVALUE);
    check_init_from_self(val, decl);
    if (!expr_assignable_to(decl->ty, val))
        diag_incompatible_init(val->loc, decl->ty, val);
    else
        warn_value_conversion(val->loc, decl->ty, val);
}

static void sema_brace_init(Node *decl)
{
    Node *flat;

    if (!decl->init || decl->init->kind != ND_INIT_LIST)
        return;

    if (!type_is_aggregate(decl->ty)) {
        sema_scalar_brace_init(decl);
        return;
    }

    if (type_is_struct(decl->ty) && !type_struct_is_complete(decl->ty)) {
        diag_error_at(decl->loc,
                      "variable '%s' has incomplete type '%s'",
                      decl->name, type_name(decl->ty));
        return;
    }

    flat = flatten_brace_init(decl->ty, decl->init, decl->loc);
    decl->init = flat;

    resolve_init_list(flat, CTX_RVALUE);
    check_init_list_from_self(flat, decl);
    check_flat_init_type(decl->ty, flat->body, decl->loc, decl);
}

static void sema_finish_function_types(Function *fn)
{
    fn->ret_ty = typedef_resolve_type(fn->ret_ty, fn->loc);

    if (type_is_struct(fn->ret_ty) && type_struct_is_complete(fn->ret_ty))
        diag_error_at(fn->loc,
                      "returning struct type '%s' by value is not supported",
                      type_name(fn->ret_ty));

    for (Param *p = fn->params; p; p = p->next) {
        if (p->decl) {
            if (p->decl_spec)
                p->decl_spec = typedef_resolve_spec(p->decl_spec, fn->loc);
            p->ty = type_apply_declarator_cb(p->decl_spec, p->decl, fn->loc,
                                             sema_array_bound_eval, NULL);
            p->decl = NULL;
            p->decl_spec = NULL;
        } else if (p->ty) {
            p->ty = typedef_resolve_type(p->ty, fn->loc);
        }
        if (type_is_struct(p->ty) && type_struct_is_complete(p->ty))
            diag_error_at(fn->loc,
                          "passing struct type '%s' by value is not supported",
                          type_name(p->ty));
    }
    func_rebuild_type(fn);
}

static void resolve_expr_inner(Node *n, ExprCtx ctx);

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

    /* C89 3.2.2.1: arrays and functions do not decay inside sizeof. */
    if (ctx == CTX_RVALUE && type_is_array(n->ty)) {
        n->ty = type_decay(n->ty);
        n->is_lvalue = 0;
        n->var_decay = 1;
    }
}

static long sizeof_value(Type *ty, SourceLoc loc)
{
    if (!ty || type_is_void(ty)) {
        diag_error_at(loc, "invalid application of sizeof to void type");
        return 1;
    }
    if (ty->kind == TY_FUNC) {
        diag_error_at(loc, "invalid application of sizeof to function type");
        return 1;
    }
    if (!type_is_complete(ty)) {
        diag_error_at(loc, "invalid application of sizeof to incomplete type");
        return 1;
    }
    return type_size(ty);
}

/* On any error we still set a fallback type so later checks don't dereference
 * a NULL Type. */
static void resolve_expr_inner(Node *n, ExprCtx ctx)
{
    if (!n)
        return;

    switch (n->kind) {
    case ND_NUM:
        /* C89 3.1.5: an L/l suffix forces long; hex/octal use unsigned typing;
         * unsuffixed decimal is int if it fits, otherwise long. */
        if (n->has_long_suffix)
            n->ty = type_long();
        else if (n->is_hex_literal)
            n->ty = type_classify_hex_constant((unsigned long)n->val);
        else if (n->is_octal_literal)
            n->ty = type_classify_octal_constant((unsigned long)n->val);
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
        if (!scope_lookup(n->name, &off, &decl_ty)) {
            FuncSym *fs = (ctx == CTX_SIZEOF_OPERAND) ? functab_find(n->name)
                                                      : NULL;

            if (fs) {
                n->ty = fs->ty;
                n->is_lvalue = 1;
            } else {
                diag_error_at(n->loc, "use of undeclared identifier '%s'",
                              n->name);
                n->ty = type_int();
                n->is_lvalue = 0;
            }
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

        if (scope_lookup(n->name, &off, NULL)) {
            diag_error_at(n->loc, "called object '%s' is not a function",
                          n->name);
        } else {
            s = functab_find(n->name);
            if (!s) {
                s = functab_add(n->name, type_func(type_int(), NULL, 0, 0), 0, 1,
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
            if (struct_cmp_error(n->lhs, n->rhs))
                diag_error_at(n->loc,
                              "invalid operands to comparison");
            else if (!eq_operands_compatible(n->lhs, n->rhs)) {
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
            if (struct_cmp_error(n->lhs, n->rhs))
                diag_error_at(n->loc,
                              "invalid operands to comparison");
            else if (!rel_operands_compatible(n->lhs, n->rhs))
                diag_error_at(n->loc,
                              "invalid operands to relational operator");
            n->ty = type_int();
        } else if (n->op == OP_COMMA) {
            n->ty = n->rhs->ty;
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
        else if (n->operand->kind == ND_VAR)
            scope_mark_address_taken(n->operand->name);
        else if (n->operand->kind == ND_MEMBER) {
            Type *sty = n->operand->lhs->ty;
            Member *m = &sty->members[n->operand->member_index];

            if (m->is_bitfield && m->bit_width > 0)
                diag_error_at(n->loc, "cannot take address of bit-field");
            else
                mark_member_base_address_taken(n->operand->lhs);
        }
        n->ty = type_ptr(n->operand->ty);
        n->is_lvalue = 0;
        return;
    case ND_MEMBER:
        resolve_expr_ctx(n->lhs, CTX_LVALUE);
        n->var_decay = 0;
        if (!member_owner_struct(n->lhs)) {
            if (n->lhs->kind != ND_DEREF)
                diag_error_at(n->loc,
                              "member reference base type is not a structure or union");
            n->ty = type_int();
            n->is_lvalue = 0;
            return;
        }
        {
            Type *sty = n->lhs->ty;
            int idx = -1;
            Member *m = type_struct_member(sty, n->name, &idx);

            if (!m) {
                diag_error_at(n->loc,
                              "no member named '%s' in '%s'",
                              n->name, type_name(sty));
                n->ty = type_int();
                n->is_lvalue = 0;
                return;
            }
            n->member_index = idx;
            n->ty = m->ty;
            n->is_lvalue = expr_is_modifiable_lvalue(n->lhs);
        }
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
        Type *dst = typedef_resolve_type(n->cast_ty, n->loc);

        resolve_expr_ctx(n->operand, CTX_RVALUE);
        n->var_decay = 0;
        n->is_lvalue = 0;
        n->cast_ty = dst;

        /* C89 3.3.4: the target must be void, scalar, or a struct whose member
         * tree is scalar-only (B11). Unless the target is void, the operand must
         * also have scalar type. */
        if (type_is_void(dst)) {
            /* (void)e discards the value; any operand type is fine. */
        } else if (!type_cast_target_ok(dst)) {
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
    case ND_SIZEOF: {
        Type *ty = typedef_resolve_type(n->cast_ty, n->loc);

        if (n->operand) {
            resolve_expr_ctx(n->operand, CTX_SIZEOF_OPERAND);
            ty = n->operand->ty;
            n->operand = NULL;
        }
        n->cast_ty = NULL;
        n->kind = ND_NUM;
        n->val = sizeof_value(ty, n->loc);
        n->has_long_suffix = 1;
        n->is_hex_literal = 0;
        n->is_octal_literal = 0;
        n->ty = type_unsigned_long();
        n->is_lvalue = 0;
        n->var_decay = 0;
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
        if (s->decl) {
            if (s->decl_spec)
                s->decl_spec = typedef_resolve_spec(s->decl_spec, s->loc);
            s->ty = type_apply_declarator_cb(s->decl_spec, s->decl, s->loc,
                                             sema_array_bound_eval, NULL);
            s->decl = NULL;
            s->decl_spec = NULL;
        }
        if (type_is_struct(s->ty) && !type_struct_is_complete(s->ty))
            diag_error_at(s->loc,
                          "variable '%s' has incomplete type '%s'",
                          s->name, type_name(s->ty));
        else if (!type_is_object(s->ty) || type_is_void(s->ty) ||
            (type_is_array(s->ty) && type_is_void(type_array_elem(s->ty))))
            diag_error_at(s->loc,
                          "variable '%s' has non-object type '%s'",
                          s->name, type_name(s->ty));
        /* Infer `T a[] = {...}` bound from its brace initializer. */
        if (type_is_array(s->ty) && type_array_count(s->ty) == 0 &&
            s->init && s->init->kind == ND_INIT_LIST)
            infer_unsized_array(s);
        if (type_is_array(s->ty) && type_array_count(s->ty) == 0)
            diag_error_at(s->loc,
                          "array size is missing or zero for '%s'",
                          s->name);
        if (scope_declared_here(s->name)) {
            SourceLoc prev;
            scope_lookup_loc_here(s->name, &prev);
            diag_error_at(s->loc, "redeclaration of '%s'", s->name);
            diag_note_at(prev, "previous declaration of '%s' is here", s->name);
            scope_lookup(s->name, &s->offset, NULL);
        } else {
            s->offset = scope_add_local(s->name, s->ty, s->loc);
        }
        /* C89 3.1.2.1: the name is in scope within its own initializer. */
        if (s->init && s->init->kind == ND_INIT_LIST)
            sema_brace_init(s);
        else {
            resolve_expr_ctx(s->init, CTX_RVALUE);
            if (s->init)
                check_init_from_self(s->init, s);
            if (s->init && !expr_assignable_to(s->ty, s->init))
                diag_incompatible_init(s->loc, s->ty, s->init);
            else if (s->init)
                warn_value_conversion(s->loc, s->ty, s->init);
        }
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
        typedef_enter_scope();
        resolve_stmt_list(s->body);
        typedef_leave_scope();
        leave_scope();
        return;
    case ND_TYPEDEF:
        /* Bound during parse for disambiguation; sema re-binds idempotently. */
        typedef_declare(s->decl_spec, s->decl, s->loc);
        s->decl = NULL;
        s->decl_spec = NULL;
        return;
    default:
        return;
    }
}

static void sema_function(Function *fn)
{
    scope_reset();
    typedef_enter_scope();
    cur_ret_ty = fn->ret_ty;
    cur_fname = fn->name;

    enter_scope();

    /* Parameters become the outermost locals. The first six live in argument
     * registers and get spilled into negative slots; the rest are passed on
     * the stack and referenced in place at positive frame-pointer offsets. */
    int i = 0;
    for (Param *p = fn->params; p; p = p->next, i++) {
        Type *pty = type_decay(p->ty);

        if (i < 6)
            p->offset = scope_alloc_local(pty);
        else
            p->offset = 16 + 8 * (i - 6);
        if (p->name) {
            if (scope_declared_here(p->name))
                diag_error_at(fn->loc, "redefinition of parameter '%s'",
                              p->name);
            else
                scope_bind(p->name, pty, p->offset, fn->loc);
        }
    }

    resolve_stmt_list(fn->body);
    scope_export_frame_locals(&fn->frame_locals, &fn->nframe_locals);
    leave_scope();
    typedef_leave_scope();

    int locals_size = scope_frame_size();

    if (locals_size < 0 || locals_size > INT_MAX - 15) {
        diag_error_at(fn->loc, "stack frame for '%s' is too large", fn->name);
        locals_size = 0;
    }
    fn->locals_size = locals_size;
}

void sema(Function *prog)
{
    functab_reset();
    typedef_reset();

    for (TypedefDecl *td = g_typedef_decls; td; td = td->next)
        typedef_declare(td->spec, td->decl, td->loc);

    /* Single forward pass: each function's declaration becomes visible before
     * its body is resolved (so recursion works), and calls to not-yet-seen
     * names fall to C89 implicit declaration. */
    for (Function *fn = prog; fn; fn = fn->next) {
        sema_finish_function_types(fn);
        functab_register(fn);
        if (fn->is_definition)
            sema_function(fn);
    }
}
