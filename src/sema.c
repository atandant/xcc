/* SPDX-License-Identifier: MIT */
#include "sema.h"
#include "sema_scope.h"
#include "sema_functab.h"
#include "sema_typedef.h"
#include "sema_enum.h"
#include "abi_sysv_amd64.h"
#include "diag.h"
#include "intconst.h"
#include "arena.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* UNDEFER: -Wnarrowing for unsigned-to-signed assignment with out-of-range constants. */
/* UNDEFER: reject comparisons between pointers and unsigned integers (stricter C89). */

/* return-statement context for the function being resolved */
static Type *cur_ret_ty;
static const char *cur_fname;
static GlobalObject *block_static_objects;
static GlobalObject *block_static_tail;
static int next_block_static_id;

/* ---- resolution ---- */

typedef enum {
    CTX_RVALUE,
    CTX_LVALUE,
    CTX_ADDR_OPERAND,
    CTX_SIZEOF_OPERAND
} ExprCtx;

static void resolve_expr_ctx(Node *n, ExprCtx ctx);
static void require_scalar_cond(Node *cond);
static long sizeof_value(Type *ty, SourceLoc loc);
static void sema_static_initializer(GlobalObject *object);

/* Make an implicit integer conversion explicit in the typed AST.  Keeping the
 * conversion in the tree gives constant folding and runtime lowering the same
 * input, rather than relying on each consumer to reconstruct C's conversion
 * rules independently. */
static void convert_integer_expr(Node **np, Type *dst)
{
    Node *old;
    Node *cast;

    if (!np || !*np || !dst)
        return;
    old = *np;
    if (!type_is_integer(old->ty) || !type_is_integer(dst) ||
        type_same(old->ty, dst))
        return;

    cast = node_cast(dst, old, old->loc);
    cast->ty = dst;
    cast->is_lvalue = 0;
    cast->next = old->next;
    old->next = NULL;
    *np = cast;
}

static int is_arith_op(BinOp op)
{
    return op == OP_ADD || op == OP_SUB || op == OP_MUL ||
           op == OP_DIV || op == OP_MOD;
}

static int is_bitwise_op(BinOp op)
{
    return op == OP_BITAND || op == OP_BITXOR || op == OP_BITOR;
}

static int is_shift_op(BinOp op)
{
    return op == OP_SHL || op == OP_SHR;
}

static int is_eq_op(BinOp op)
{
    return op == OP_EQ || op == OP_NE;
}

static int is_void_ptr_type(Type *ty)
{
    return type_is_pointer(ty) && type_is_void(ty->base);
}

static int sema_ice_lookup(const char *name, long *out, Type **out_ty,
                           void *ctx)
{
    (void)ctx;
    if (!enum_const_lookup(name, out))
        return 0;
    if (out_ty)
        *out_ty = type_int();
    return 1;
}

static int sema_ice_sizeof(Node *n, long *out, void *ctx)
{
    Type *ty = n->cast_ty;

    (void)ctx;
    if (n->operand) {
        resolve_expr_ctx(n->operand, CTX_SIZEOF_OPERAND);
        ty = n->operand->ty;
    }
    if (!ty)
        return 0;
    *out = sizeof_value(ty, n->loc);
    return 1;
}

/* C89 3.4 / 3.2.2.3: evaluate an integral constant expression (ICE). */
static int ice_eval(Node *n, long *out)
{
    return int_const_eval(n, sema_ice_lookup, sema_ice_sizeof, NULL,
                          out, NULL);
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
    if (type_is_pointer(dst) && is_null_ptr_constant(src))
        return 1;
    if (type_is_pointer(dst) && type_is_function_pointer(src->ty) &&
        is_void_ptr_type(dst))
        return 1;
    if (type_is_function_pointer(dst) && type_is_pointer(src->ty) &&
        is_void_ptr_type(src->ty))
        return 1;
    return 0;
}

/* char storage is unsigned byte-wide (movzbl); warn on out-of-range constants. */
static void warn_value_conversion(SourceLoc loc, Type *dst, Node *src)
{
    if (!dst || !src || !src->ty)
        return;

    if (type_is_plain_char(dst) && type_is_integer(src->ty) && !type_is_char(src->ty)) {
        if (src->kind == ND_NUM) {
            if (src->val < 0 || src->val > 255)
                diag_warn(W_CHAR_CONSTANT_OVERFLOW, loc,
                          "overflow in conversion from '%s' to '%s'",
                          type_name(src->ty), type_name(dst));
        } else {
            diag_warn(W_CHAR_VALUE_NARROWING, loc,
                      "conversion from '%s' to '%s' may alter value",
                      type_name(src->ty), type_name(dst));
        }
    }

    if (type_is_pointer(dst) && type_is_pointer(src->ty) &&
        !type_same(dst, src->ty)) {
        int dst_fn = dst->base && dst->base->kind == TY_FUNC;
        int src_fn = src->ty->base && src->ty->base->kind == TY_FUNC;

        if (dst_fn || src_fn) {
            if (is_void_ptr_type(dst) || is_void_ptr_type(src->ty))
                diag_warn(W_IMPLICIT_VOID_POINTER, loc,
                          "conversion between '%s' and '%s' without a cast",
                          type_name(src->ty), type_name(dst));
        } else if (type_assignable(dst, src->ty) &&
                   (is_void_ptr_type(dst) || is_void_ptr_type(src->ty))) {
            diag_warn(W_IMPLICIT_VOID_POINTER, loc,
                      "conversion from '%s' to '%s' without a cast",
                      type_name(src->ty), type_name(dst));
        }
    }
}

static void check_init_from_self(Node *n, Node *decl)
{
    if (!n || !decl)
        return;

    if (n->kind == ND_VAR && strcmp(n->name, decl->name) == 0 &&
        n->offset == decl->offset) {
        diag_warn(W_SELF_REFERENTIAL_INITIALIZER, n->loc,
                  "initializer refers to '%s' before its value is defined",
                  decl->name);
        diag_note_at(n->loc,
                     "'%s' has indeterminate value here; behavior is undefined in C89",
                     decl->name);
        return;
    }

    switch (n->kind) {
    case ND_BINOP:
    case ND_LOGAND:
    case ND_LOGOR:
        check_init_from_self(n->lhs, decl);
        check_init_from_self(n->rhs, decl);
        return;
    case ND_COND:
        check_init_from_self(n->cond, decl);
        check_init_from_self(n->then_expr, decl);
        check_init_from_self(n->else_expr, decl);
        return;
    case ND_NEG:
    case ND_NOT:
    case ND_PREINC:
    case ND_PREDEC:
    case ND_POSTINC:
    case ND_POSTDEC:
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
        check_init_from_self(n->callee, decl);
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

static int member_base_is_register(Node *n)
{
    while (n && n->kind == ND_MEMBER)
        n = n->lhs;
    return n && n->kind == ND_VAR && scope_is_register(n->name);
}

static Type *member_owner_record(Node *base)
{
    if (!base || !base->ty)
        return NULL;
    if (type_is_record(base->ty))
        return base->ty;
    return NULL;
}

static int record_cmp_error(Node *lhs, Node *rhs)
{
    return type_is_record(lhs->ty) || type_is_record(rhs->ty);
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
    if (type_is_function_pointer(lhs->ty) || type_is_function_pointer(rhs->ty))
        return 0;
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
    return type_is_array(t) || type_is_record(t);
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
    if (type_is_union(t)) {
        if (t->nmembers > 0)
            return type_init_slot_count(t->members[0].ty);
        return 1;
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
    if (type_is_record(t)) {
        int i, n = type_is_union(t) ? 1 : t->nmembers;

        for (i = 0; i < n; i++)
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

static int init_char_array_string(Type *t, Node **pcursor, InitFlat *flat,
                                  SourceLoc loc)
{
    Node *string = *pcursor;
    Type *elem = type_array_elem(t);
    int count = type_array_count(t);
    int i;

    if (!string || string->kind != ND_STRING || !type_is_char(elem))
        return 0;
    *pcursor = string->next;
    if (string->string_len > count) {
        diag_error_at(loc, "character string literal is too long for array");
        return -1;
    }
    for (i = 0; i < count; i++) {
        long value = i < string->string_len ? string->string_data[i] : 0;

        flat_append(flat, node_num(value, string->loc));
    }
    return 1;
}

static int init_aggregate(Type *t, Node **pcursor, InitFlat *flat,
                          SourceLoc loc, int *excess)
{
    if (type_is_array(t)) {
        Type *elem = type_array_elem(t);
        int n = type_array_count(t);
        int i;
        int string_init = init_char_array_string(t, pcursor, flat, loc);

        if (string_init)
            return string_init < 0;

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

    if (type_is_union(t)) {
        Type *mty;

        if (!type_struct_is_complete(t)) {
            diag_error_at(loc,
                          "initializer element is not computable for "
                          "incomplete type '%s'", type_name(t));
            return 1;
        }
        if (t->nmembers == 0) {
            pad_aggregate_zeros(type_int(), flat, loc);
            return 0;
        }
        mty = t->members[0].ty;
        if (!*pcursor) {
            pad_aggregate_zeros(mty, flat, loc);
            return 0;
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
        if (type_is_record(aty))
            diag_error_at(loc, "excess elements in struct or union initializer");
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
    if (type_is_record(t)) {
        int i, n = type_is_union(t) ? 1 : t->nmembers;

        for (i = 0; i < n; i++)
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
    else {
        warn_value_conversion(val->loc, decl->ty, val);
        convert_integer_expr(&decl->init, decl->ty);
    }
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

    if (type_is_record(decl->ty) && !type_struct_is_complete(decl->ty)) {
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

    if (type_is_record(fn->ret_ty) && !type_struct_is_complete(fn->ret_ty))
        diag_error_at(fn->loc,
                      "return type '%s' is incomplete",
                      type_name(fn->ret_ty));

    for (Param *p = fn->params; p; p = p->next) {
        Type *declared_ty;

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
        declared_ty = p->ty;
        if (type_is_void(declared_ty)) {
            diag_error_at(fn->loc,
                          "parameter must not have void type");
        } else if (type_is_array(declared_ty) &&
                   !type_is_complete(type_array_elem(declared_ty))) {
            diag_error_at(fn->loc,
                          "array parameter has incomplete element type '%s'",
                          type_name(type_array_elem(declared_ty)));
        } else if (type_is_record(declared_ty) &&
                   !type_struct_is_complete(declared_ty)) {
            diag_error_at(fn->loc,
                          "parameter type '%s' is incomplete",
                          type_name(declared_ty));
        }

        for (Param *prev = fn->params; prev != p; prev = prev->next) {
            if (p->name && prev->name && strcmp(p->name, prev->name) == 0) {
                diag_error_at(fn->loc, "redefinition of parameter '%s'",
                              p->name);
                break;
            }
        }
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
        convert_integer_expr(&n->lhs, n->ty);
        convert_integer_expr(&n->rhs, n->ty);
        return 1;
    }

    if (n->op == OP_ADD) {
        if (type_is_function_pointer(l) || type_is_function_pointer(r))
            return 0;
        /* C89 3.3.6: integer operand may be char, int, or long (after
         * integral promotion for narrower types). */
        if (type_is_pointer(l) && type_is_integer(r)) {
            convert_integer_expr(&n->rhs, type_int_promote(r));
            n->ty = l;
            return 1;
        }
        if (type_is_integer(l) && type_is_pointer(r)) {
            convert_integer_expr(&n->lhs, type_int_promote(l));
            n->ty = r;
            return 1;
        }
    }

    if (n->op == OP_SUB) {
        if (type_is_function_pointer(l) || type_is_function_pointer(r))
            return 0;
        if (type_is_pointer(l) && type_is_integer(r)) {
            convert_integer_expr(&n->rhs, type_int_promote(r));
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

static int check_bitwise_binop(Node *n)
{
    Type *common;

    if (!type_is_integer(n->lhs->ty) || !type_is_integer(n->rhs->ty))
        return 0;
    common = type_arith_convert(n->lhs->ty, n->rhs->ty);
    convert_integer_expr(&n->lhs, common);
    convert_integer_expr(&n->rhs, common);
    n->ty = common;
    return 1;
}

static int check_shift_binop(Node *n)
{
    Type *left;
    Type *right;

    if (!type_is_integer(n->lhs->ty) || !type_is_integer(n->rhs->ty))
        return 0;
    left = type_int_promote(n->lhs->ty);
    right = type_int_promote(n->rhs->ty);
    convert_integer_expr(&n->lhs, left);
    convert_integer_expr(&n->rhs, right);
    n->ty = left;
    return 1;
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

    /* C89 3.2.2.1: arrays and functions do not decay inside sizeof or &. */
    if (ctx == CTX_RVALUE) {
        if (type_is_array(n->ty)) {
            n->ty = type_decay(n->ty);
            n->is_lvalue = 0;
            n->var_decay = 1;
        } else if (n->ty && n->ty->kind == TY_FUNC) {
            n->ty = type_decay(n->ty);
            n->is_lvalue = 0;
            n->func_decay = 1;
        }
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
        n->ty = n->is_char_constant ? type_int() :
            type_classify_integer_constant(
                n->val, n->has_long_suffix, n->has_unsigned_suffix,
                n->is_hex_literal || n->is_octal_literal);
        n->is_lvalue = 0;
        n->var_decay = 0;
        return;
    case ND_STRING:
        n->ty = type_array(type_char(), n->string_len + 1);
        n->is_lvalue = ctx != CTX_RVALUE;
        n->var_decay = 0;
        n->func_decay = 0;
        return;
    case ND_VAR: {
        ScopeBinding *binding;
        long enum_val;

        n->var_decay = 0;
        n->func_decay = 0;
        n->storage = VAR_STORAGE_NONE;
        binding = scope_lookup_binding(n->name);
        if (!binding) {
            FuncSym *fs = filesym_find(n->name);

            if (fs) {
                n->ty = fs->ty;
                if (fs->kind == FILESYM_FUNCTION) {
                    n->storage = VAR_STORAGE_FUNCTION;
                    n->symbol_name = fs->name;
                    n->is_lvalue = 0;
                    if (!fs->referenced) {
                        fs->referenced = 1;
                        fs->reference_loc = n->loc;
                    }
                } else {
                    n->storage = VAR_STORAGE_GLOBAL;
                    n->symbol_name = fs->name;
                    n->is_lvalue = (ctx != CTX_RVALUE) && type_is_object(fs->ty);
                }
            } else if (enum_const_lookup(n->name, &enum_val)) {
                n->kind = ND_NUM;
                n->val = enum_val;
                n->has_long_suffix = 0;
                n->has_unsigned_suffix = 0;
                n->is_hex_literal = 0;
                n->is_octal_literal = 0;
                n->is_char_constant = 0;
                n->ty = type_int();
                n->is_lvalue = 0;
            } else {
                diag_error_at(n->loc, "use of undeclared identifier '%s'",
                              n->name);
                n->ty = type_int();
                n->is_lvalue = 0;
            }
        } else if (binding->kind == SCOPE_AUTO_OBJECT) {
            n->offset = binding->offset;
            n->ty = binding->ty;
            n->storage = VAR_STORAGE_LOCAL;
            n->is_lvalue = (ctx != CTX_RVALUE) && type_is_object(binding->ty);
        } else if (binding->kind == SCOPE_FUNCTION) {
            n->ty = binding->ty;
            n->storage = VAR_STORAGE_FUNCTION;
            n->symbol_name = binding->symbol_name;
            n->is_lvalue = 0;
            if (binding->entity && !binding->entity->referenced) {
                binding->entity->referenced = 1;
                binding->entity->reference_loc = n->loc;
            }
        } else {
            n->ty = binding->ty;
            n->storage = VAR_STORAGE_GLOBAL;
            n->symbol_name = binding->symbol_name;
            n->is_lvalue = (ctx != CTX_RVALUE) && type_is_object(binding->ty);
        }
        return;
    }
    case ND_CALL: {
        FuncSym *s = NULL;
        Type *fty = NULL;

        n->ty = type_int();
        n->func_ty = NULL;
        n->call_direct = 0;
        n->is_lvalue = 0;
        n->var_decay = 0;
        if (n->nargs > XCC_MAX_CALL_ARGS) {
            diag_error_at(n->loc, "too many arguments in function call");
        }
        if (!n->callee) {
            diag_error_at(n->loc, "called object is not a function");
            return;
        }
        if (n->callee->kind == ND_VAR) {
            if (!scope_lookup_binding(n->callee->name) &&
                !filesym_find(n->callee->name)) {
                Type *implicit_ty = type_func(type_int(), NULL, 0, 0);
                FuncSym *imp = entity_declare(n->callee->name, implicit_ty,
                                              FILESYM_FUNCTION,
                                              LINKAGE_EXTERNAL, n->loc);
                if (imp && imp->kind == FILESYM_FUNCTION) {
                    imp->implicit = 1;
                    scope_bind_linked(n->callee->name, imp->ty,
                                      SCOPE_FUNCTION, imp, n->loc);
                    diag_warn(W_IMPLICIT_FUNCTION_DECLARATION, n->loc,
                              "implicit declaration of function '%s'",
                              n->callee->name);
                }
            }
        }
        resolve_expr_ctx(n->callee, CTX_RVALUE);
        for (Node *a = n->args; a; a = a->next)
            resolve_expr_ctx(a, CTX_RVALUE);

        if (n->callee->kind == ND_VAR) {
            if (n->callee->storage == VAR_STORAGE_FUNCTION) {
                fty = type_is_function_pointer(n->callee->ty)
                    ? n->callee->ty->base : n->callee->ty;
                s = entity_find(n->callee->name);
                n->call_direct = 1;
                n->name = n->callee->symbol_name
                    ? n->callee->symbol_name : n->callee->name;
            } else if (type_is_function_pointer(n->callee->ty)) {
                fty = n->callee->ty->base;
            } else {
                diag_error_at(n->loc, "called object '%s' is not a function",
                              n->callee->name);
            }
        } else if (type_is_function_pointer(n->callee->ty)) {
            fty = n->callee->ty->base;
        } else {
            diag_error_at(n->loc, "called object is not a function or function pointer");
        }

        if (fty) {
            if (s) {
                if (!s->ty->prototyped && !s->implicit)
                    diag_warn(W_CALL_WITHOUT_PROTOTYPE, n->loc,
                              "call to function '%s' without a prototype",
                              s->name);
            } else if (!fty->prototyped) {
                diag_warn(W_CALL_WITHOUT_PROTOTYPE, n->loc,
                          "call to function pointer without a prototype");
            }
            if (fty->prototyped && fty->nparams != n->nargs) {
                const char *callee_name = n->name ? n->name : "function";

                if (n->nargs < fty->nparams)
                    diag_error_at(n->loc,
                                  "too few arguments to function '%s'",
                                  callee_name);
                else
                    diag_error_at(n->loc,
                                  "too many arguments to function '%s'",
                                  callee_name);
            } else if (fty->prototyped) {
                int i = 0;
                const char *callee_name = n->name ? n->name : "function pointer";

                for (Node **ap = &n->args; *ap; ap = &(*ap)->next, i++) {
                    Node *a = *ap;

                    if (!expr_assignable_to(fty->params[i], a))
                        diag_error_at(a->loc,
                                      "passing '%s' to parameter of type '%s' in call to '%s'",
                                      type_name(a->ty),
                                      type_name(fty->params[i]),
                                      callee_name);
                    else {
                        warn_value_conversion(a->loc, fty->params[i], a);
                        convert_integer_expr(ap, fty->params[i]);
                    }
                }
            } else {
                /* C89 3.3.2.2: calls without a prototype apply the default
                 * argument promotions.  XCC has no floating types yet, so the
                 * integral promotions are the complete supported subset. */
                for (Node **ap = &n->args; *ap; ap = &(*ap)->next)
                    convert_integer_expr(ap, type_int_promote((*ap)->ty));
            }
            n->func_ty = fty;
            n->ty = fty->ret;
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
        else {
            warn_value_conversion(n->loc, n->lhs->ty, n->rhs);
            convert_integer_expr(&n->rhs, n->lhs->ty);
        }
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
        } else if (is_bitwise_op(n->op)) {
            if (!check_bitwise_binop(n))
                diag_error_at(n->loc,
                              "invalid operands to bitwise operator");
        } else if (is_shift_op(n->op)) {
            if (!check_shift_binop(n))
                diag_error_at(n->loc,
                              "invalid operands to shift operator");
        } else if (is_eq_op(n->op)) {
            if (record_cmp_error(n->lhs, n->rhs))
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
            } else if (type_is_integer(n->lhs->ty) &&
                       type_is_integer(n->rhs->ty)) {
                Type *common = type_arith_convert(n->lhs->ty, n->rhs->ty);
                convert_integer_expr(&n->lhs, common);
                convert_integer_expr(&n->rhs, common);
            }
            n->ty = type_int();
        } else if (is_rel_op(n->op)) {
            if (record_cmp_error(n->lhs, n->rhs))
                diag_error_at(n->loc,
                              "invalid operands to comparison");
            else if (!rel_operands_compatible(n->lhs, n->rhs))
                diag_error_at(n->loc,
                              "invalid operands to relational operator");
            else if (type_is_integer(n->lhs->ty) &&
                     type_is_integer(n->rhs->ty)) {
                Type *common = type_arith_convert(n->lhs->ty, n->rhs->ty);
                convert_integer_expr(&n->lhs, common);
                convert_integer_expr(&n->rhs, common);
            }
            n->ty = type_int();
        } else if (n->op == OP_COMMA) {
            n->ty = n->rhs->ty;
        }
        n->is_lvalue = 0;
        return;
    case ND_NOT:
        resolve_expr_ctx(n->operand, CTX_RVALUE);
        require_scalar_cond(n->operand);
        n->ty = type_int();
        n->is_lvalue = 0;
        return;
    case ND_LOGAND:
    case ND_LOGOR:
        resolve_expr_ctx(n->lhs, CTX_RVALUE);
        resolve_expr_ctx(n->rhs, CTX_RVALUE);
        require_scalar_cond(n->lhs);
        require_scalar_cond(n->rhs);
        n->ty = type_int();
        n->is_lvalue = 0;
        return;
    case ND_COND:
        resolve_expr_ctx(n->cond, CTX_RVALUE);
        require_scalar_cond(n->cond);
        resolve_expr_ctx(n->then_expr, CTX_RVALUE);
        resolve_expr_ctx(n->else_expr, CTX_RVALUE);
        if (type_is_integer(n->then_expr->ty) &&
            type_is_integer(n->else_expr->ty)) {
            Type *common = type_arith_convert(n->then_expr->ty,
                                              n->else_expr->ty);
            convert_integer_expr(&n->then_expr, common);
            convert_integer_expr(&n->else_expr, common);
            n->ty = common;
        } else if (type_same(n->then_expr->ty, n->else_expr->ty)) {
            n->ty = n->then_expr->ty;
        } else if (type_is_pointer(n->then_expr->ty) &&
                   is_null_ptr_constant(n->else_expr)) {
            n->ty = n->then_expr->ty;
        } else if (is_null_ptr_constant(n->then_expr) &&
                   type_is_pointer(n->else_expr->ty)) {
            n->ty = n->else_expr->ty;
        } else if (type_is_pointer(n->then_expr->ty) &&
                   type_is_pointer(n->else_expr->ty) &&
                   (is_void_ptr_type(n->then_expr->ty) ||
                    is_void_ptr_type(n->else_expr->ty))) {
            n->ty = is_void_ptr_type(n->then_expr->ty)
                ? n->then_expr->ty : n->else_expr->ty;
        } else {
            diag_error_at(n->loc,
                          "incompatible operands to conditional operator");
            n->ty = n->then_expr->ty;
        }
        n->is_lvalue = 0;
        return;
    case ND_NEG:
        resolve_expr_ctx(n->operand, CTX_RVALUE);
        n->var_decay = 0;
        if (!type_is_integer(n->operand->ty))
            diag_error_at(n->loc, "invalid operand to unary minus");
        n->ty = type_int_promote(n->operand->ty);
        convert_integer_expr(&n->operand, n->ty);
        n->is_lvalue = 0;
        return;
    case ND_PREINC:
    case ND_PREDEC:
    case ND_POSTINC:
    case ND_POSTDEC:
        resolve_expr_ctx(n->operand, CTX_LVALUE);
        n->var_decay = 0;
        if (!expr_is_modifiable_lvalue(n->operand))
            diag_error_at(n->loc,
                          "operand of increment/decrement is not a modifiable lvalue");
        else if (type_is_integer(n->operand->ty))
            n->ty = type_int_promote(n->operand->ty);
        else if (type_is_pointer(n->operand->ty))
            n->ty = n->operand->ty;
        else
            diag_error_at(n->loc, "invalid operand to increment/decrement");
        n->is_lvalue = 0;
        return;
    case ND_ADDR:
        resolve_expr_ctx(n->operand, CTX_ADDR_OPERAND);
        n->var_decay = 0;
        if (n->operand->ty && n->operand->ty->kind == TY_FUNC) {
            n->ty = type_ptr(n->operand->ty);
            n->is_lvalue = 0;
            return;
        }
        if (!expr_is_lvalue(n->operand))
            diag_error_at(n->loc, "cannot take address of non-lvalue");
        else if (n->operand->kind == ND_VAR) {
            if (scope_is_register(n->operand->name))
                diag_error_at(n->loc,
                              "cannot take address of register object '%s'",
                              n->operand->name);
            else
                scope_mark_address_taken(n->operand->name);
        }
        else if (n->operand->kind == ND_MEMBER) {
            Type *sty = n->operand->lhs->ty;
            Member *m = &sty->members[n->operand->member_index];

            if (m->is_bitfield && m->bit_width > 0)
                diag_error_at(n->loc, "cannot take address of bit-field");
            else if (member_base_is_register(n->operand))
                diag_error_at(n->loc,
                              "cannot take address of member of register object");
            else
                mark_member_base_address_taken(n->operand->lhs);
        }
        n->ty = type_ptr(n->operand->ty);
        n->is_lvalue = 0;
        return;
    case ND_MEMBER:
        resolve_expr_ctx(n->lhs, CTX_LVALUE);
        n->var_decay = 0;
        if (!member_owner_record(n->lhs)) {
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
        } else if (n->operand->ty->base->kind == TY_FUNC) {
            n->ty = n->operand->ty->base;
            n->is_lvalue = 0;
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

static int loop_depth;

typedef struct SwitchCtx SwitchCtx;
struct SwitchCtx {
    Node *node;
    Node *last_case;
    Type *control_ty;
    SwitchCtx *prev;
};

static SwitchCtx *current_switch;
static Node *function_labels;

static Node *find_label(const char *name)
{
    for (Node *label = function_labels; label; label = label->label_next)
        if (strcmp(label->name, name) == 0)
            return label;
    return NULL;
}

static void collect_labels(Node *s, Function *fn);

static void collect_label_list(Node *body, Function *fn)
{
    for (Node *s = body; s; s = s->next)
        collect_labels(s, fn);
}

static void collect_labels(Node *s, Function *fn)
{
    Node *previous;

    if (!s)
        return;
    switch (s->kind) {
    case ND_LABEL:
        previous = find_label(s->name);
        if (previous) {
            diag_error_at(s->loc, "duplicate label '%s'", s->name);
            diag_note_at(previous->loc, "previous label is here");
        } else {
            s->label_next = function_labels;
            function_labels = s;
        }
        collect_labels(s->then_body, fn);
        return;
    case ND_GOTO:
        fn->has_goto = 1;
        return;
    case ND_IF:
        collect_labels(s->then_body, fn);
        collect_labels(s->else_body, fn);
        return;
    case ND_WHILE:
    case ND_DO_WHILE:
    case ND_FOR:
    case ND_SWITCH:
    case ND_CASE:
    case ND_DEFAULT:
        collect_labels(s->then_body, fn);
        return;
    case ND_BLOCK:
        collect_label_list(s->body, fn);
        return;
    default:
        return;
    }
}

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

static Linkage block_extern_linkage(const char *name)
{
    ScopeBinding *binding = scope_lookup_binding(name);
    FuncSym *file_symbol;

    if (binding)
        return binding->entity ? binding->entity->linkage : LINKAGE_EXTERNAL;
    file_symbol = filesym_find(name);
    return file_symbol ? file_symbol->linkage : LINKAGE_EXTERNAL;
}

static FuncSym *declare_block_linked(Node *decl, FileSymKind kind)
{
    ScopeBinding *here = scope_lookup_binding_here(decl->name);
    Linkage linkage = block_extern_linkage(decl->name);
    FuncSym *entity;

    if (here && (!here->entity ||
        (kind == FILESYM_FUNCTION && here->kind != SCOPE_FUNCTION) ||
        (kind == FILESYM_OBJECT && here->kind != SCOPE_LINKED_OBJECT))) {
        diag_error_at(decl->loc, "redeclaration of '%s'", decl->name);
        diag_note_at(here->loc, "previous declaration of '%s' is here",
                     decl->name);
        return NULL;
    }

    entity = entity_declare(decl->name, decl->ty, kind, linkage, decl->loc);
    if (here && entity)
        here->ty = entity->ty;
    else if (entity)
        scope_bind_linked(decl->name, entity->ty,
                          kind == FILESYM_FUNCTION
                              ? SCOPE_FUNCTION : SCOPE_LINKED_OBJECT,
                          entity, decl->loc);
    return entity;
}

static GlobalObject *new_block_static(Node *decl)
{
    char label[64];
    GlobalObject *object = arena_alloc_zeroed(sizeof(*object));

    snprintf(label, sizeof(label), ".L.xcc.static.%d", next_block_static_id++);
    object->name = arena_strdup(label);
    object->source_name = decl->name;
    object->loc = decl->loc;
    object->storage = STORAGE_STATIC;
    object->linkage = LINKAGE_NONE;
    object->decl_kind = OBJECT_DEFINITION;
    object->ty = decl->ty;
    object->init = decl->init;
    object->emit = 1;
    if (block_static_tail)
        block_static_tail->next_static = object;
    else
        block_static_objects = object;
    block_static_tail = object;
    return object;
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
        if (typedef_lookup(s->name) && typedef_declared_here(s->name))
            diag_error_at(s->loc, "redeclared '%s' as different kind of symbol",
                          s->name);
        typedef_hide_name(s->name, s->loc);
        if (s->ty && s->ty->kind == TY_FUNC) {
            if (s->decl_storage == STORAGE_STATIC ||
                s->decl_storage == STORAGE_AUTO ||
                s->decl_storage == STORAGE_REGISTER)
                diag_error_at(s->loc,
                              "invalid storage class for block-scope function '%s'",
                              s->name);
            if (s->init)
                diag_error_at(s->loc, "function '%s' is initialized like an object",
                              s->name);
            if (s->decl_storage != STORAGE_STATIC)
                (void)declare_block_linked(s, FILESYM_FUNCTION);
            return;
        }
        if (s->decl_storage == STORAGE_EXTERN) {
            if (s->init)
                diag_error_at(s->loc,
                              "block-scope extern object '%s' cannot have an initializer",
                              s->name);
            if (!type_is_object(s->ty) || type_is_void(s->ty))
                diag_error_at(s->loc, "extern object '%s' has non-object type '%s'",
                              s->name, type_name(s->ty));
            (void)declare_block_linked(s, FILESYM_OBJECT);
            return;
        }
        if (s->decl_storage == STORAGE_STATIC) {
            ScopeBinding *here = scope_lookup_binding_here(s->name);
            GlobalObject *object;

            if (here) {
                diag_error_at(s->loc, "redeclaration of '%s'", s->name);
                diag_note_at(here->loc, "previous declaration of '%s' is here",
                             s->name);
                return;
            }
            object = new_block_static(s);
            scope_bind_static(s->name, s->ty, object->name, s->loc);
            s->symbol_name = object->name;
            sema_static_initializer(object);
            s->ty = object->ty;
            s->init = NULL;
            return;
        }
        if (type_is_record(s->ty) && !type_struct_is_complete(s->ty))
            diag_error_at(s->loc,
                          "variable '%s' has incomplete type '%s'",
                          s->name, type_name(s->ty));
        else if (!type_is_object(s->ty) || type_is_void(s->ty) ||
            (type_is_array(s->ty) && type_is_void(type_array_elem(s->ty))))
            diag_error_at(s->loc,
                          "variable '%s' has non-object type '%s'",
                          s->name, type_name(s->ty));
        /* C89 3.5.7: an ordinary string initializes a character array,
           including its trailing null when the declared bound has room. */
        if (type_is_array(s->ty) && type_is_char(type_array_elem(s->ty)) &&
            s->init && s->init->kind == ND_STRING) {
            if (type_array_count(s->ty) == 0) {
                s->ty->count = s->init->string_len + 1;
                s->ty->size = type_size(type_array_elem(s->ty)) * s->ty->count;
            }
            s->init = node_init_list(s->init, s->init->loc);
        }
        if (type_is_array(s->ty) && type_is_char(type_array_elem(s->ty)) &&
            type_array_count(s->ty) == 0 && s->init &&
            s->init->kind == ND_INIT_LIST && s->init->body &&
            s->init->body->kind == ND_STRING && !s->init->body->next) {
            s->ty->count = s->init->body->string_len + 1;
            s->ty->size = type_size(type_array_elem(s->ty)) * s->ty->count;
        }
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
            s->offset = scope_add_local(
                s->name, s->ty, s->loc,
                s->decl_storage == STORAGE_REGISTER);
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
            else if (s->init) {
                warn_value_conversion(s->loc, s->ty, s->init);
                convert_integer_expr(&s->init, s->ty);
            }
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
            } else if (!type_is_void(cur_ret_ty)) {
                warn_value_conversion(s->loc, cur_ret_ty, s->operand);
                convert_integer_expr(&s->operand, cur_ret_ty);
            }
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
    case ND_BREAK:
        if (loop_depth == 0 && !current_switch)
            diag_error_at(s->loc, "break statement not within loop or switch");
        return;
    case ND_CONTINUE:
        if (loop_depth == 0)
            diag_error_at(s->loc, "continue statement not within loop");
        return;
    case ND_WHILE:
    case ND_DO_WHILE:
        resolve_expr_ctx(s->cond, CTX_RVALUE);
        require_scalar_cond(s->cond);
        loop_depth++;
        resolve_stmt(s->then_body);
        loop_depth--;
        return;
    case ND_FOR:
        resolve_expr_ctx(s->init, CTX_RVALUE);
        resolve_expr_ctx(s->cond, CTX_RVALUE);
        require_scalar_cond(s->cond);
        resolve_expr_ctx(s->step, CTX_RVALUE);
        loop_depth++;
        resolve_stmt(s->then_body);
        loop_depth--;
        return;
    case ND_SWITCH: {
        SwitchCtx sw = {
            .node = s,
            .control_ty = type_int(),
            .prev = current_switch,
        };
        resolve_expr_ctx(s->cond, CTX_RVALUE);
        if (!type_is_integer(s->cond->ty))
            diag_error_at(s->cond->loc,
                          "switch quantity is not an integer");
        else {
            sw.control_ty = type_int_promote(s->cond->ty);
            convert_integer_expr(&s->cond, sw.control_ty);
        }
        current_switch = &sw;
        resolve_stmt(s->then_body);
        current_switch = sw.prev;
        return;
    }
    case ND_CASE: {
        long value;
        resolve_expr_ctx(s->operand, CTX_RVALUE);
        if (!current_switch) {
            diag_error_at(s->loc, "case label not within a switch statement");
        } else if (!type_is_integer(s->operand->ty)) {
            diag_error_at(s->loc, "case label does not have integer type");
        } else if (!ice_eval(s->operand, &value)) {
            diag_error_at(s->loc,
                          "case label is not an integer constant expression");
        } else {
            Node *prev;
            value = type_convert_const(value, current_switch->control_ty);
            for (prev = current_switch->node->cases; prev;
                 prev = prev->case_next) {
                if (prev->case_val == value) {
                    diag_error_at(s->loc, "duplicate case value");
                    diag_note_at(prev->loc, "previous case is here");
                    break;
                }
            }
            s->case_val = value;
            if (current_switch->last_case)
                current_switch->last_case->case_next = s;
            else
                current_switch->node->cases = s;
            current_switch->last_case = s;
        }
        resolve_stmt(s->then_body);
        return;
    }
    case ND_DEFAULT:
        if (!current_switch) {
            diag_error_at(s->loc,
                          "default label not within a switch statement");
        } else if (current_switch->node->default_case) {
            diag_error_at(s->loc, "multiple default labels in one switch");
            diag_note_at(current_switch->node->default_case->loc,
                         "previous default is here");
        } else {
            current_switch->node->default_case = s;
        }
        resolve_stmt(s->then_body);
        return;
    case ND_LABEL:
        resolve_stmt(s->then_body);
        return;
    case ND_GOTO:
        s->goto_target = find_label(s->name);
        if (!s->goto_target)
            diag_error_at(s->loc, "label '%s' used but not defined", s->name);
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

static void sema_scan_call_member_scratch(Node *n, int *maxsz)
{
    if (!n || !maxsz)
        return;

    if (n->kind == ND_MEMBER && n->lhs && n->lhs->kind == ND_CALL &&
        abi_type_is_record_pass(n->lhs->ty)) {
        int sz = type_size(n->lhs->ty);
        if (sz > *maxsz)
            *maxsz = sz;
    }

    switch (n->kind) {
    case ND_MEMBER:
        sema_scan_call_member_scratch(n->lhs, maxsz);
        return;
    case ND_CALL:
        for (Node *a = n->args; a; a = a->next) {
            if (a->kind == ND_CALL && abi_type_is_record_pass(a->ty)) {
                int sz = type_size(a->ty);
                if (sz > *maxsz)
                    *maxsz = sz;
            }
            sema_scan_call_member_scratch(a, maxsz);
        }
        return;
    case ND_ASSIGN:
        sema_scan_call_member_scratch(n->lhs, maxsz);
        sema_scan_call_member_scratch(n->rhs, maxsz);
        return;
    case ND_RETURN:
    case ND_EXPR_STMT:
    case ND_NEG:
    case ND_NOT:
    case ND_PREINC:
    case ND_PREDEC:
    case ND_POSTINC:
    case ND_POSTDEC:
    case ND_ADDR:
    case ND_DEREF:
    case ND_CAST:
        sema_scan_call_member_scratch(n->operand, maxsz);
        return;
    case ND_BINOP:
    case ND_LOGAND:
    case ND_LOGOR:
        sema_scan_call_member_scratch(n->lhs, maxsz);
        sema_scan_call_member_scratch(n->rhs, maxsz);
        return;
    case ND_COND:
        sema_scan_call_member_scratch(n->cond, maxsz);
        sema_scan_call_member_scratch(n->then_expr, maxsz);
        sema_scan_call_member_scratch(n->else_expr, maxsz);
        return;
    case ND_DECL:
        sema_scan_call_member_scratch(n->init, maxsz);
        return;
    case ND_INIT_LIST:
        for (Node *e = n->body; e; e = e->next)
            sema_scan_call_member_scratch(e, maxsz);
        return;
    case ND_IF:
        sema_scan_call_member_scratch(n->cond, maxsz);
        sema_scan_call_member_scratch(n->then_body, maxsz);
        sema_scan_call_member_scratch(n->else_body, maxsz);
        return;
    case ND_WHILE:
    case ND_DO_WHILE:
    case ND_FOR:
        sema_scan_call_member_scratch(n->init, maxsz);
        sema_scan_call_member_scratch(n->cond, maxsz);
        sema_scan_call_member_scratch(n->step, maxsz);
        sema_scan_call_member_scratch(n->then_body, maxsz);
        return;
    case ND_SWITCH:
        sema_scan_call_member_scratch(n->cond, maxsz);
        sema_scan_call_member_scratch(n->then_body, maxsz);
        return;
    case ND_CASE:
        sema_scan_call_member_scratch(n->operand, maxsz);
        sema_scan_call_member_scratch(n->then_body, maxsz);
        return;
    case ND_DEFAULT:
        sema_scan_call_member_scratch(n->then_body, maxsz);
        return;
    case ND_LABEL:
        sema_scan_call_member_scratch(n->then_body, maxsz);
        return;
    case ND_GOTO:
        return;
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next)
            sema_scan_call_member_scratch(s, maxsz);
        return;
    default:
        return;
    }
}

static void sema_scan_sret_call_scratch(Node *n, int *maxsz);

static int sret_call_needs_scratch(Node *call)
{
    AbiRetPlan rp;
    Type *ret;

    if (!call || call->kind != ND_CALL || !call->func_ty)
        return 0;
    ret = call->func_ty->ret;
    if (!abi_type_is_record_pass(ret))
        return 0;
    abi_ret_plan(ret, &rp);
    return rp.kind == ABI_RET_SRET;
}

static void sema_scan_sret_call_args(Node *call, int *maxsz)
{
    if (!call || call->kind != ND_CALL)
        return;
    for (Node *a = call->args; a; a = a->next)
        sema_scan_sret_call_scratch(a, maxsz);
}

static void sema_scan_sret_call_scratch(Node *n, int *maxsz)
{
    if (!n || !maxsz)
        return;

    switch (n->kind) {
    case ND_CALL:
        if (sret_call_needs_scratch(n)) {
            int sz = type_size(n->func_ty->ret);
            if (sz > *maxsz)
                *maxsz = sz;
        }
        sema_scan_sret_call_args(n, maxsz);
        return;
    case ND_ASSIGN:
        if (n->rhs && n->rhs->kind == ND_CALL && sret_call_needs_scratch(n->rhs))
            sema_scan_sret_call_args(n->rhs, maxsz);
        else
            sema_scan_sret_call_scratch(n->rhs, maxsz);
        return;
    case ND_RETURN:
        if (n->operand && n->operand->kind == ND_CALL &&
            sret_call_needs_scratch(n->operand))
            sema_scan_sret_call_args(n->operand, maxsz);
        else
            sema_scan_sret_call_scratch(n->operand, maxsz);
        return;
    case ND_EXPR_STMT:
    case ND_NEG:
    case ND_NOT:
    case ND_ADDR:
    case ND_DEREF:
    case ND_CAST:
        sema_scan_sret_call_scratch(n->operand, maxsz);
        return;
    case ND_BINOP:
    case ND_LOGAND:
    case ND_LOGOR:
        sema_scan_sret_call_scratch(n->lhs, maxsz);
        sema_scan_sret_call_scratch(n->rhs, maxsz);
        return;
    case ND_COND:
        sema_scan_sret_call_scratch(n->cond, maxsz);
        sema_scan_sret_call_scratch(n->then_expr, maxsz);
        sema_scan_sret_call_scratch(n->else_expr, maxsz);
        return;
    case ND_DECL:
        if (n->init && n->init->kind == ND_CALL && sret_call_needs_scratch(n->init))
            sema_scan_sret_call_args(n->init, maxsz);
        else
            sema_scan_sret_call_scratch(n->init, maxsz);
        return;
    case ND_INIT_LIST:
        for (Node *e = n->body; e; e = e->next)
            sema_scan_sret_call_scratch(e, maxsz);
        return;
    case ND_IF:
        sema_scan_sret_call_scratch(n->cond, maxsz);
        sema_scan_sret_call_scratch(n->then_body, maxsz);
        sema_scan_sret_call_scratch(n->else_body, maxsz);
        return;
    case ND_WHILE:
    case ND_DO_WHILE:
    case ND_FOR:
        sema_scan_sret_call_scratch(n->init, maxsz);
        sema_scan_sret_call_scratch(n->cond, maxsz);
        sema_scan_sret_call_scratch(n->step, maxsz);
        sema_scan_sret_call_scratch(n->then_body, maxsz);
        return;
    case ND_SWITCH:
        sema_scan_sret_call_scratch(n->cond, maxsz);
        sema_scan_sret_call_scratch(n->then_body, maxsz);
        return;
    case ND_CASE:
        sema_scan_sret_call_scratch(n->operand, maxsz);
        sema_scan_sret_call_scratch(n->then_body, maxsz);
        return;
    case ND_DEFAULT:
        sema_scan_sret_call_scratch(n->then_body, maxsz);
        return;
    case ND_LABEL:
        sema_scan_sret_call_scratch(n->then_body, maxsz);
        return;
    case ND_GOTO:
        return;
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next)
            sema_scan_sret_call_scratch(s, maxsz);
        return;
    default:
        return;
    }
}

static void sema_function(Function *fn)
{
    AbiRetPlan ret_plan;

    scope_reset();
    function_labels = NULL;
    fn->has_goto = 0;
    collect_label_list(fn->body, fn);
    typedef_enter_scope();
    cur_ret_ty = fn->ret_ty;
    cur_fname = fn->name;

    enter_scope();

    abi_ret_plan(fn->ret_ty, &ret_plan);
    fn->abi_ret_sret = (ret_plan.kind == ABI_RET_SRET);
    fn->abi_sret_offset = 0;
    fn->abi_call_scratch = 0;

    if (fn->abi_ret_sret)
        fn->abi_sret_offset = scope_alloc_local(type_ptr(type_void()));

    int gpr_slot = fn->abi_ret_sret ? 1 : 0;
    int stack_off = 0;

    for (Param *p = fn->params; p; p = p->next) {
        Type *pty = type_decay(p->ty);

        p->abi_stack_bytes = 0;
        if (abi_type_is_record_pass(pty)) {
            AbiArgPlan ap;

            abi_arg_plan(pty, &ap);
            if (!abi_arg_fits_gprs(&ap, gpr_slot, 6)) {
                p->offset = 16 + stack_off;
                p->abi_gpr_start = -1;
                p->abi_ngpr = 0;
                p->abi_stack_bytes = abi_stack_arg_bytes(ap.size);
                stack_off += p->abi_stack_bytes;
            } else {
                p->offset = scope_alloc_local(pty);
                p->abi_gpr_start = gpr_slot;
                p->abi_ngpr = ap.ngpr;
                gpr_slot += ap.ngpr;
            }
        } else if (gpr_slot < 6) {
            p->offset = scope_alloc_local(pty);
            p->abi_gpr_start = gpr_slot;
            p->abi_ngpr = 1;
            gpr_slot++;
        } else {
            p->offset = 16 + stack_off;
            p->abi_gpr_start = -1;
            p->abi_ngpr = 0;
            p->abi_stack_bytes = 8;
            stack_off += 8;
        }

        if (p->name) {
            if (!scope_declared_here(p->name))
                scope_bind(p->name, pty, p->offset, fn->loc,
                           p->storage == STORAGE_REGISTER);
        }
    }

    resolve_stmt_list(fn->body);

    {
        int max_scratch = 0;
        Node *s;

        for (s = fn->body; s; s = s->next) {
            sema_scan_sret_call_scratch(s, &max_scratch);
            sema_scan_call_member_scratch(s, &max_scratch);
        }
        if (max_scratch > 0)
            fn->abi_call_scratch = scope_alloc_local(
                type_array(type_char(), max_scratch));
    }

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

static void static_write_integer(unsigned char *data, int offset, int width,
                                 unsigned long value)
{
    for (int i = 0; i < width; i++)
        data[offset + i] = (unsigned char)(value >> (i * 8));
}

static unsigned long static_read_integer(const unsigned char *data, int offset,
                                         int width)
{
    unsigned long value = 0;

    for (int i = 0; i < width; i++)
        value |= (unsigned long)data[offset + i] << (i * 8);
    return value;
}

static void static_add_reloc(GlobalObject *object, int offset, char *symbol,
                             long addend)
{
    StaticReloc *reloc = arena_alloc_zeroed(sizeof(*reloc));
    StaticReloc **tail = &object->relocs;

    reloc->offset = offset;
    reloc->width = 8;
    reloc->symbol = symbol;
    reloc->addend = addend;
    while (*tail)
        tail = &(*tail)->next;
    *tail = reloc;
}

static int static_address_expr(Node *n, char **symbol, long *addend);

static int static_lvalue_address(Node *n, char **symbol, long *addend)
{
    if (!n)
        return 0;
    switch (n->kind) {
    case ND_STRING:
        *symbol = n->string_label;
        *addend = 0;
        return 1;
    case ND_VAR:
        if (n->storage != VAR_STORAGE_GLOBAL &&
            n->storage != VAR_STORAGE_FUNCTION)
            return 0;
        *symbol = n->symbol_name ? n->symbol_name : n->name;
        *addend = 0;
        return 1;
    case ND_DEREF:
        return static_address_expr(n->operand, symbol, addend);
    case ND_MEMBER: {
        Type *owner = n->lhs ? n->lhs->ty : NULL;
        Member *member;

        if (!type_is_record(owner) || n->member_index < 0 ||
            n->member_index >= owner->nmembers ||
            !static_lvalue_address(n->lhs, symbol, addend))
            return 0;
        member = &owner->members[n->member_index];
        if (member->is_bitfield)
            return 0;
        *addend += member->offset;
        return 1;
    }
    default:
        return 0;
    }
}

static int static_scaled_addend(Node *pointer, Node *index, int subtract,
                                char **symbol, long *addend)
{
    long value;
    int scale;

    if (!static_address_expr(pointer, symbol, addend) ||
        !ice_eval(index, &value) || !type_is_pointer(pointer->ty))
        return 0;
    scale = type_size(type_ptr_elem(pointer->ty));
    if (scale <= 0)
        return 0;
    if (value > 0 && value > LONG_MAX / scale)
        return 0;
    if (value < 0 && value < LONG_MIN / scale)
        return 0;
    value *= scale;
    if (subtract) {
        if (value == LONG_MIN)
            return 0;
        value = -value;
    }
    if ((value > 0 && *addend > LONG_MAX - value) ||
        (value < 0 && *addend < LONG_MIN - value))
        return 0;
    *addend += value;
    return 1;
}

static int static_address_expr(Node *n, char **symbol, long *addend)
{
    if (!n)
        return 0;
    switch (n->kind) {
    case ND_STRING:
        *symbol = n->string_label;
        *addend = 0;
        return 1;
    case ND_VAR:
        if ((n->storage != VAR_STORAGE_GLOBAL || !n->var_decay) &&
            (n->storage != VAR_STORAGE_FUNCTION || !n->func_decay))
            return 0;
        *symbol = n->symbol_name ? n->symbol_name : n->name;
        *addend = 0;
        return 1;
    case ND_ADDR:
        return static_lvalue_address(n->operand, symbol, addend);
    case ND_CAST:
        if (!type_is_pointer(n->ty))
            return 0;
        return static_address_expr(n->operand, symbol, addend);
    case ND_BINOP:
        if (n->op == OP_ADD) {
            if (static_scaled_addend(n->lhs, n->rhs, 0, symbol, addend))
                return 1;
            return static_scaled_addend(n->rhs, n->lhs, 0, symbol, addend);
        }
        if (n->op == OP_SUB)
            return static_scaled_addend(n->lhs, n->rhs, 1, symbol, addend);
        return 0;
    default:
        return 0;
    }
}

static int static_initializer_error(GlobalObject *object)
{
    if (object->source_name)
        diag_error_at(object->loc,
                      "initializer for block-scope static object '%s' is not constant",
                      object->source_name);
    else
        diag_error_at(object->loc,
                      "initializer for file-scope object '%s' is not constant",
                      object->name);
    return 0;
}

static int encode_static_initializer(GlobalObject *object, Type *ty,
                                     Node **pcursor, int offset)
{
    if (type_is_array(ty)) {
        Type *elem = type_array_elem(ty);
        int size = type_size(elem);

        for (int i = 0; i < type_array_count(ty); i++)
            if (!encode_static_initializer(object, elem, pcursor,
                                           offset + i * size))
                return 0;
        return 1;
    }
    if (type_is_struct(ty)) {
        for (int i = 0; i < ty->nmembers; i++) {
            Member *member = &ty->members[i];

            if (member->is_bitfield && member->bit_width > 0) {
                Node *value = *pcursor;
                long constant;
                unsigned long unit;
                unsigned long mask;

                if (!value || !ice_eval(value, &constant))
                    return static_initializer_error(object);
                *pcursor = value->next;
                constant = type_convert_const(constant, member->ty);
                unit = static_read_integer(object->init_data,
                                           offset + member->offset, 4);
                mask = member->bit_width == 32
                    ? 0xffffffffUL
                    : (1UL << member->bit_width) - 1;
                unit &= ~(mask << member->bit_offset);
                unit |= ((unsigned long)constant & mask) << member->bit_offset;
                static_write_integer(object->init_data,
                                     offset + member->offset, 4, unit);
            } else if (!encode_static_initializer(object, member->ty, pcursor,
                                                  offset + member->offset)) {
                return 0;
            }
        }
        return 1;
    }
    if (type_is_union(ty)) {
        if (ty->nmembers == 0)
            return 1;
        return encode_static_initializer(object, ty->members[0].ty, pcursor,
                                         offset);
    }
    {
        Node *value = *pcursor;
        long constant;

        if (!value)
            return static_initializer_error(object);
        *pcursor = value->next;
        if (type_is_pointer(ty)) {
            char *symbol;
            long addend;

            if (is_null_ptr_constant(value))
                return 1;
            if (!static_address_expr(value, &symbol, &addend))
                return static_initializer_error(object);
            static_add_reloc(object, offset, symbol, addend);
            return 1;
        }
        if (!type_is_integer(ty) || !ice_eval(value, &constant))
            return static_initializer_error(object);
        constant = type_convert_const(constant, ty);
        static_write_integer(object->init_data, offset, type_size(ty),
                             (unsigned long)constant);
        return 1;
    }
}

static void sema_global_object(GlobalObject *object)
{
    Node *flat = NULL;
    Node *cursor;

    if (object->decl) {
        object->decl_spec = typedef_resolve_spec(object->decl_spec, object->loc);
        object->ty = type_apply_declarator_cb(object->decl_spec, object->decl,
                                              object->loc,
                                              sema_array_bound_eval, NULL);
        object->decl = NULL;
        object->decl_spec = NULL;
    }

    if (!object->source_name &&
        (object->storage == STORAGE_AUTO ||
         object->storage == STORAGE_REGISTER))
        diag_error_at(object->loc,
                      "invalid storage class for file-scope object '%s'",
                      object->name);

    if (type_is_array(object->ty) &&
        type_is_char(type_array_elem(object->ty)) && object->init &&
        object->init->kind == ND_STRING) {
        if (type_array_count(object->ty) == 0) {
            object->ty->count = object->init->string_len + 1;
            object->ty->size = object->ty->count;
        }
        object->init = node_init_list(object->init, object->init->loc);
    }
    if (type_is_array(object->ty) &&
        type_is_char(type_array_elem(object->ty)) &&
        type_array_count(object->ty) == 0 && object->init &&
        object->init->kind == ND_INIT_LIST && object->init->body &&
        object->init->body->kind == ND_STRING && !object->init->body->next) {
        object->ty->count = object->init->body->string_len + 1;
        object->ty->size = object->ty->count;
    }
    if (type_is_array(object->ty) && type_array_count(object->ty) == 0 &&
        object->init && object->init->kind == ND_INIT_LIST) {
        Node fake = {0};

        fake.name = object->name;
        fake.loc = object->loc;
        fake.ty = object->ty;
        fake.init = object->init;
        infer_unsized_array(&fake);
    }

    if (object->source_name)
        object->decl_kind = OBJECT_DEFINITION;
    else if (object->init)
        object->decl_kind = OBJECT_DEFINITION;
    else if (object->storage == STORAGE_EXTERN)
        object->decl_kind = OBJECT_DECLARATION;
    else
        object->decl_kind = OBJECT_TENTATIVE;

    if (!type_is_object(object->ty) || type_is_void(object->ty)) {
        diag_error_at(object->loc, "%s object '%s' has non-object type '%s'",
                      object->source_name ? "block-scope static" : "file-scope",
                      object->source_name ? object->source_name : object->name,
                      type_name(object->ty));
    } else if (!type_is_complete(object->ty)) {
        int incomplete_external_array =
            object->decl_kind == OBJECT_TENTATIVE &&
            object->storage != STORAGE_STATIC && type_is_array(object->ty) &&
            type_array_count(object->ty) == 0 &&
            type_is_complete(type_array_elem(object->ty));

        if (object->decl_kind != OBJECT_DECLARATION &&
            !incomplete_external_array)
            diag_error_at(object->loc,
                          "%s object '%s' has incomplete type '%s'",
                          object->source_name ? "block-scope static" : "file-scope",
                          object->source_name ? object->source_name : object->name,
                          type_name(object->ty));
    }

    if (!object->source_name)
        objecttab_register(object);
    if (!object->init)
        return;

    if (type_is_aggregate(object->ty)) {
        if (object->init->kind != ND_INIT_LIST) {
            diag_error_at(object->loc,
                          "file-scope aggregate initializer for '%s' must be brace-enclosed",
                          object->name);
            return;
        }
        flat = flatten_brace_init(object->ty, object->init, object->loc);
        object->init = flat;
        resolve_init_list(flat, CTX_RVALUE);
        check_flat_init_type(object->ty, flat->body, object->loc, NULL);
        cursor = flat->body;
    } else {
        if (object->init->kind == ND_INIT_LIST) {
            Node *body = object->init->body;

            if (!body) {
                diag_error_at(object->loc, "empty scalar initializer");
                return;
            }
            if (body->kind == ND_INIT_LIST) {
                diag_error_at(object->loc,
                              "too many braces around scalar initializer");
                return;
            }
            if (body->next) {
                diag_error_at(object->loc,
                              "excess elements in scalar initializer");
                return;
            }
            body->next = NULL;
            object->init = body;
        }
        resolve_expr_ctx(object->init, CTX_RVALUE);
        if (!expr_assignable_to(object->ty, object->init)) {
            diag_incompatible_init(object->loc, object->ty, object->init);
            return;
        }
        warn_value_conversion(object->loc, object->ty, object->init);
        cursor = object->init;
    }

    object->init_size = type_size(object->ty);
    object->init_data = arena_alloc_zeroed((size_t)object->init_size);
    if (!encode_static_initializer(object, object->ty, &cursor, 0))
        object->init_data = NULL;
}

static void sema_static_initializer(GlobalObject *object)
{
    sema_global_object(object);
}

GlobalObject *sema_block_static_objects(void)
{
    return block_static_objects;
}

void sema(ExternalDecl *prog)
{
    functab_reset();
    typedef_reset();
    block_static_objects = NULL;
    block_static_tail = NULL;
    next_block_static_id = 0;

    for (TypedefDecl *td = g_typedef_decls; td; td = td->next)
        typedef_declare(td->spec, td->decl, td->loc);

    /* Preserve declaration order: each external name becomes visible before
       the following declaration or function body is resolved. */
    for (ExternalDecl *external = prog; external; external = external->next) {
        if (external->kind == EXT_OBJECT) {
            sema_global_object(external->object);
        } else {
            Function *fn = external->function;
            sema_finish_function_types(fn);
            if (fn->storage == STORAGE_AUTO ||
                fn->storage == STORAGE_REGISTER)
                diag_error_at(fn->loc,
                              "invalid storage class for function '%s'",
                              fn->name);
            functab_register(fn);
            if (fn->is_definition)
                sema_function(fn);
        }
    }
    functab_finalize();
    objecttab_finalize();
}
