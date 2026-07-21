/* SPDX-License-Identifier: MIT */
#include "ast.h"
#include "arena.h"
#include "diag.h"
#include "intconst.h"
#include "sema_enum.h"

#include <limits.h>
#include <string.h>

static Node *new_node(NodeKind kind, SourceLoc loc)
{
    Node *n = arena_alloc_zeroed(sizeof(Node));
    n->kind = kind;
    n->loc = loc;
    return n;
}

Node *node_num(long v, SourceLoc loc)
{
    Node *n = new_node(ND_NUM, loc);
    n->val = v;
    return n;
}

Node *node_var(char *name, SourceLoc loc)
{
    Node *n = new_node(ND_VAR, loc);
    n->name = name;
    return n;
}

Node *node_binop(BinOp op, Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_BINOP, loc);
    n->op = op;
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_neg(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_NEG, loc);
    n->operand = o;
    return n;
}

Node *node_not(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_NOT, loc);
    n->operand = o;
    return n;
}

Node *node_logand(Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_LOGAND, loc);
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_logor(Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_LOGOR, loc);
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_cond(Node *cond, Node *then_expr, Node *else_expr, SourceLoc loc)
{
    Node *n = new_node(ND_COND, loc);
    n->cond = cond;
    n->then_expr = then_expr;
    n->else_expr = else_expr;
    return n;
}

Node *node_addr(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_ADDR, loc);
    n->operand = o;
    return n;
}

Node *node_deref(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_DEREF, loc);
    n->operand = o;
    return n;
}

Node *node_member(Node *base, char *name, SourceLoc loc)
{
    Node *n = new_node(ND_MEMBER, loc);
    n->lhs = base;
    n->name = name;
    return n;
}

Node *node_cast(Type *ty, Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_CAST, loc);
    n->cast_ty = ty;
    n->operand = o;
    return n;
}

Node *node_sizeof_expr(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_SIZEOF, loc);
    n->operand = o;
    return n;
}

Node *node_sizeof_type(Type *ty, SourceLoc loc)
{
    Node *n = new_node(ND_SIZEOF, loc);
    n->cast_ty = ty;
    return n;
}

Node *node_assign(Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_ASSIGN, loc);
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_return(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_RETURN, loc);
    n->operand = o;
    return n;
}

Node *node_expr_stmt(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_EXPR_STMT, loc);
    n->operand = o;
    return n;
}

Node *node_decl(char *name, Type *spec_ty, Declarator *decl, Node *init,
                SourceLoc loc)
{
    Node *n = new_node(ND_DECL, loc);
    n->name = name;
    n->decl_spec = spec_ty;
    n->decl = decl;
    n->init = init;
    return n;
}

Node *node_init_list(Node *items, SourceLoc loc)
{
    Node *n = new_node(ND_INIT_LIST, loc);
    n->body = items;
    return n;
}

Node *init_list_append(Node *head, Node *item)
{
    item->next = NULL;
    if (!head)
        return item;
    Node *t = head;
    while (t->next)
        t = t->next;
    t->next = item;
    return head;
}

Node *node_call(Node *callee, NodeList *args, SourceLoc loc)
{
    Node *n = new_node(ND_CALL, loc);
    n->callee = callee;
    n->name = (callee && callee->kind == ND_VAR) ? callee->name : NULL;
    n->call_direct = 0;
    n->args = stmt_list_head(args);
    n->nargs = 0;
    for (Node *a = n->args; a; a = a->next)
        n->nargs++;
    return n;
}

Node *node_if(Node *cond, Node *then_body, Node *else_body, SourceLoc loc)
{
    Node *n = new_node(ND_IF, loc);
    n->cond = cond;
    n->then_body = then_body;
    n->else_body = else_body;
    return n;
}

Node *node_while(Node *cond, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_WHILE, loc);
    n->cond = cond;
    n->then_body = body;
    return n;
}

Node *node_do_while(Node *body, Node *cond, SourceLoc loc)
{
    Node *n = new_node(ND_DO_WHILE, loc);
    n->cond = cond;
    n->then_body = body;
    return n;
}

Node *node_for(Node *init, Node *cond, Node *step, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_FOR, loc);
    n->init = init;
    n->cond = cond;
    n->step = step;
    n->then_body = body;
    return n;
}

Node *node_switch(Node *cond, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_SWITCH, loc);
    n->cond = cond;
    n->then_body = body;
    return n;
}

Node *node_case(Node *expr, Node *stmt, SourceLoc loc)
{
    Node *n = new_node(ND_CASE, loc);
    n->operand = expr;
    n->then_body = stmt;
    return n;
}

Node *node_default(Node *stmt, SourceLoc loc)
{
    Node *n = new_node(ND_DEFAULT, loc);
    n->then_body = stmt;
    return n;
}

Node *node_label(char *name, Node *stmt, SourceLoc loc)
{
    Node *n = new_node(ND_LABEL, loc);
    n->name = name;
    n->then_body = stmt;
    n->label = -1;
    return n;
}

Node *node_goto(char *name, SourceLoc loc)
{
    Node *n = new_node(ND_GOTO, loc);
    n->name = name;
    return n;
}

Node *node_break(SourceLoc loc)
{
    return new_node(ND_BREAK, loc);
}

Node *node_continue(SourceLoc loc)
{
    return new_node(ND_CONTINUE, loc);
}

Node *node_block(Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_BLOCK, loc);
    n->body = body;
    return n;
}

TypedefDecl *g_typedef_decls;

TypedefDecl *typedef_decl_new(Type *spec, Declarator *decl, SourceLoc loc)
{
    TypedefDecl *td = arena_alloc_zeroed(sizeof(*td));
    td->spec = spec;
    td->decl = decl;
    td->loc = loc;
    return td;
}

Node *node_typedef(Type *spec, Declarator *decl, SourceLoc loc)
{
    Node *n = new_node(ND_TYPEDEF, loc);
    n->decl_spec = spec;
    n->decl = decl;
    return n;
}

TypedefDecl *typedef_decl_append(TypedefDecl *list, Type *spec, Declarator *decl,
                                 SourceLoc loc)
{
    TypedefDecl *td = arena_alloc_zeroed(sizeof(*td));
    td->spec = spec;
    td->decl = decl;
    td->loc = loc;
    td->next = NULL;

    if (!list)
        return td;

    TypedefDecl *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = td;
    return list;
}

StructField *struct_field_append(StructField *list, Type *spec, Declarator *decl,
                                 SourceLoc loc)
{
    StructField *f = arena_alloc_zeroed(sizeof(*f));
    f->spec = spec;
    f->decl = decl;
    f->loc = loc;

    if (!list)
        return f;

    StructField *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = f;
    return list;
}

StructField *struct_field_append_bit(StructField *list, Type *spec, Declarator *decl,
                                     Node *bit_width, SourceLoc loc)
{
    if (!decl)
        decl = declarator_empty();
    decl = declarator_bitfield(decl, bit_width);
    return struct_field_append(list, spec, decl, loc);
}

Enumerator *enumerator_new(char *name, Node *value, SourceLoc loc)
{
    Enumerator *e = arena_alloc_zeroed(sizeof(*e));
    e->name = name;
    e->value = value;
    e->loc = loc;
    e->next = NULL;
    return e;
}

Enumerator *enumerator_append(Enumerator *list, Enumerator *e)
{
    Enumerator *tail;

    if (!list)
        return e;
    tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = e;
    return list;
}

static void member_append(Member **members, int *nmembers, Member *src)
{
    Member *m;
    int n = *nmembers;

    m = arena_alloc((size_t)(n + 1) * sizeof(*m));
    if (n > 0)
        memcpy(m, *members, (size_t)n * sizeof(*m));
    m[n] = *src;
    *members = m;
    (*nmembers)++;
}

Member *struct_fields_to_members(StructField *fields, int *out_n, SourceLoc loc)
{
    Member *members = NULL;
    int nmembers = 0;
    StructField *f;

    for (f = fields; f; f = f->next) {
        char *name = f->decl ? declarator_name(f->decl) : NULL;
        Type *mty;
        Member m = {0};

        if (!f->decl || !f->decl->bit_width_expr) {
            if (!name) {
                diag_error_at(f->loc, "struct member requires a name");
                continue;
            }
            mty = type_apply_declarator(f->spec, f->decl, f->loc);
        } else if (f->decl) {
            mty = type_apply_declarator(f->spec, f->decl, f->loc);
        } else {
            mty = f->spec ? f->spec : type_int();
        }
        m.name = name;
        m.ty = mty;
        m.is_bitfield = f->decl && f->decl->bit_width_expr != NULL;
        member_append(&members, &nmembers, &m);
    }

    if (out_n)
        *out_n = nmembers;
    (void)loc;
    return members;
}

struct NodeList {
    Node *head;
    Node *tail;
};

NodeList *stmt_list_new(void)
{
    return arena_alloc_zeroed(sizeof(NodeList));
}

NodeList *stmt_list_append(NodeList *list, Node *s)
{
    if (!list)
        list = stmt_list_new();
    if (!list->head)
        list->head = s;
    else
        list->tail->next = s;
    list->tail = s;
    return list;
}

Node *stmt_list_head(NodeList *list)
{
    return list ? list->head : NULL;
}

Param *param_append(Param *list, Type *ty, char *name)
{
    Param *p = arena_alloc_zeroed(sizeof(Param));
    p->name = name;
    p->ty = ty;
    if (!list)
        return p;
    Param *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = p;
    return list;
}

Param *param_append_decl(Param *list, Type *spec_ty, Declarator *decl,
                         char *name)
{
    Param *p = arena_alloc_zeroed(sizeof(Param));
    p->name = name;
    p->decl_spec = spec_ty;
    p->decl = decl;
    if (!list)
        return p;
    Param *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = p;
    return list;
}

ParamClause *param_clause(Param *head, int prototyped)
{
    ParamClause *pc = arena_alloc_zeroed(sizeof(ParamClause));
    pc->head = head;
    pc->prototyped = prototyped;
    for (Param *p = head; p; p = p->next)
        pc->count++;
    return pc;
}

Function *func_new(char *name, ParamClause *pc, Type *ret_ty,
                   int is_definition, Node *body, SourceLoc loc)
{
    Function *f = arena_alloc_zeroed(sizeof(Function));
    f->name = name;
    f->loc = loc;
    f->params = pc->head;
    f->nparams = pc->count;
    f->prototyped = pc->prototyped;
    f->ret_ty = ret_ty;
    f->is_definition = is_definition;
    f->body = body;
    f->ty = type_func(ret_ty, NULL, 0, pc->prototyped);
    return f;
}

Function *func_rebuild_type(Function *fn)
{
    Type **ptypes = NULL;
    int n = fn->nparams;

    if (n > 0) {
        ptypes = arena_alloc(sizeof(Type *) * (size_t)n);
        int i = 0;
        for (Param *p = fn->params; p; p = p->next, i++)
            ptypes[i] = type_decay(p->ty);
    }
    fn->ty = type_func(fn->ret_ty, ptypes, n, fn->prototyped);
    return fn;
}

Function *func_append(Function *list, Function *f)
{
    if (!list)
        return f;
    Function *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = f;
    return list;
}

Declarator *declarator_empty(void)
{
    return arena_alloc_zeroed(sizeof(Declarator));
}

Declarator *declarator_ident(char *name)
{
    Declarator *d = declarator_empty();
    d->name = name;
    return d;
}

Declarator *declarator_bitfield(Declarator *d, Node *width)
{
    d->bit_width_expr = width;
    return d;
}

Declarator *declarator_ptr(Declarator *d)
{
    d->nptr++;
    return d;
}

static int decl_add_suffix_dim(Declarator *d, Node *dim, SourceLoc loc)
{
    if (d->ndims_suffix >= MAX_DECL_DIMS) {
        diag_error_at(loc, "too many array dimensions");
        return 0;
    }
    d->dims_suffix[d->ndims_suffix++] = dim;
    return 1;
}

Declarator *declarator_suffix(Declarator *d, Node *dim)
{
    (void)decl_add_suffix_dim(d, dim, (SourceLoc){ 0, 0 });
    return d;
}

Declarator *declarator_add_dim(Declarator *d, Node *dim, int after_paren)
{
    if (after_paren)
        return declarator_paren_outer(d, dim);
    if (d->ndims_paren_outer > 0) {
        if (d->ndims_paren_outer >= MAX_DECL_DIMS)
            return d;
        d->dims_paren_outer[d->ndims_paren_outer++] = dim;
        return d;
    }
    return declarator_suffix(d, dim);
}

Declarator *declarator_paren_group(Declarator *d)
{
    Declarator *wrap = declarator_empty();
    wrap->inner = d;
    wrap->was_paren = 1;
    return wrap;
}

Declarator *declarator_paren_outer(Declarator *d, Node *dim)
{
    Declarator *wrap = declarator_empty();
    wrap->inner = d->inner;
    wrap->dims_paren_outer[0] = dim;
    wrap->ndims_paren_outer = 1;
    return wrap;
}

Declarator *declarator_func(Declarator *d, ParamClause *pc)
{
    if (!d)
        d = declarator_empty();
    d->func_params = pc;
    return d;
}

int declarator_was_paren(const Declarator *d)
{
    return d && d->was_paren;
}

char *declarator_name(const Declarator *d)
{
    for (const Declarator *cur = d; cur; cur = cur->inner) {
        if (cur->name)
            return cur->name;
    }
    return NULL;
}

static long ast_sizeof_type(Type *ty, SourceLoc loc)
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

static int parse_abstract_sizeof(Node *expr, long *out, void *ctx)
{
    SourceLoc *loc = ctx;

    if (!expr || expr->operand || !loc)
        return 0;
    *out = ast_sizeof_type(expr->cast_ty, *loc);
    return 1;
}

static int parse_abstract_ice_lookup(const char *name, long *out, Type **out_ty,
                                     void *ctx)
{
    (void)ctx;
    if (!enum_const_lookup(name, out))
        return 0;
    if (out_ty)
        *out_ty = type_int();
    return 1;
}

static int parse_abstract_dim_eval(Node *expr, long *out, SourceLoc loc, void *ctx)
{
    (void)ctx;
    if (!expr) {
        *out = 0;
        return 1;
    }
    if (!int_const_eval(expr, parse_abstract_ice_lookup, parse_abstract_sizeof,
                      &loc, out, NULL)) {
        diag_error_at(loc, "array size is not an integer constant expression");
        return 0;
    }
    return 1;
}

static int eval_decl_dim(Node *expr, long *out, SourceLoc loc, DeclDimEvalFn eval,
                         void *ctx)
{
    if (!expr) {
        *out = 0;
        return 1;
    }
    if (eval)
        return eval(expr, out, loc, ctx);
    if (expr->kind == ND_NUM) {
        *out = expr->val;
        return 1;
    }
    diag_error_at(loc, "array size is not an integer constant expression");
    return 0;
}

static Type *make_array_dim(Type *ty, Node *dim, SourceLoc loc,
                            DeclDimEvalFn eval, void *ctx)
{
    int count;
    int esz;
    long bound;

    if (!eval_decl_dim(dim, &bound, loc, eval, ctx))
        return ty;
    if (bound == 0) {
        if (dim) {
            diag_error_at(loc, "array size is zero");
            return ty;
        }
        /* An unsized `[]` parameter decays to a pointer before allocation. */
        return type_array(ty, 0);
    }
    if (bound < 0) {
        diag_error_at(loc, "array size is negative");
        return ty;
    }
    if (bound > INT_MAX) {
        diag_error_at(loc, "array size is too large");
        return ty;
    }
    count = (int)bound;
    esz = type_size(ty);
    if (esz > 0 && count > INT_MAX / esz) {
        diag_error_at(loc, "array size overflows");
        return ty;
    }
    return type_array(ty, count);
}

static Type *type_func_from_params(Type *ret, ParamClause *pc, SourceLoc loc)
{
    Type **ptypes = NULL;
    int n = 0;

    if (pc && pc->head) {
        for (Param *p = pc->head; p; p = p->next)
            n++;
        if (n > 0) {
            ptypes = arena_alloc((size_t)n * sizeof(*ptypes));
            int i = 0;
            for (Param *p = pc->head; p; p = p->next, i++) {
                Type *pty;

                if (p->decl)
                    pty = type_apply_declarator(p->decl_spec, p->decl, loc);
                else if (p->decl_spec)
                    pty = p->decl_spec;
                else
                    pty = p->ty;

                if (type_is_void(pty)) {
                    diag_error_at(loc, "parameter must not have void type");
                } else if (type_is_array(pty) &&
                           !type_is_complete(type_array_elem(pty))) {
                    diag_error_at(loc,
                                  "array parameter has incomplete element type '%s'",
                                  type_name(type_array_elem(pty)));
                }
                for (Param *prev = pc->head; prev != p; prev = prev->next) {
                    if (p->name && prev->name &&
                        strcmp(p->name, prev->name) == 0) {
                        diag_error_at(loc, "redefinition of parameter '%s'",
                                      p->name);
                        break;
                    }
                }
                ptypes[i] = type_decay(pty);
            }
        }
    }

    /* C89: a function type cannot be returned by value; decay to pointer. */
    if (ret && ret->kind == TY_FUNC)
        ret = type_ptr(ret);
    return type_func(ret, ptypes, n, pc ? pc->prototyped : 0);
}

static Type *apply_decl_leaf(Type *base, const Declarator *d, SourceLoc loc,
                             DeclDimEvalFn eval, void *ctx)
{
    Type *ty = base;
    int i;

    if (d->inner) {
        if (d->inner->was_paren || d->inner->func_params ||
            d->inner->ndims_paren_outer > 0)
            ty = type_apply_declarator_cb(ty, d->inner, loc, eval, ctx);
        else
            ty = apply_decl_leaf(ty, d->inner, loc, eval, ctx);
    }

    for (i = 0; i < d->nptr; i++)
        ty = type_ptr(ty);
    for (i = d->ndims_suffix - 1; i >= 0; i--)
        ty = make_array_dim(ty, d->dims_suffix[i], loc, eval, ctx);
    return ty;
}

Type *type_apply_declarator_cb(Type *base, Declarator *d, SourceLoc loc,
                               DeclDimEvalFn eval, void *ctx)
{
    Type *ty;
    int i;

    if (!d)
        return base;

    if (d->inner && d->ndims_paren_outer > 0) {
        /* `int (*p)[3]`: outer `[]` binds to the base before the inner `*`. */
        ty = base;
        for (i = d->ndims_paren_outer - 1; i >= 0; i--)
            ty = make_array_dim(ty, d->dims_paren_outer[i], loc, eval, ctx);
        return apply_decl_leaf(ty, d->inner, loc, eval, ctx);
    }

    if (d->func_params) {
        /* SHELVED: nested fnptr declarators such as int (*(*x)(int))(char)
         * need a func suffix on an inner pointer-to-fnptr chain; only the
         * outermost `(...)()` suffix is handled today. Use typedefs meanwhile. */
        ty = type_func_from_params(base, d->func_params, loc);
        if (d->inner)
            return apply_decl_leaf(ty, d->inner, loc, eval, ctx);
        return ty;
    }

    if (d->was_paren && d->inner)
        return apply_decl_leaf(base, d->inner, loc, eval, ctx);

    return apply_decl_leaf(base, d, loc, eval, ctx);
}

Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc)
{
    return type_apply_declarator_cb(base, d, loc, parse_abstract_dim_eval, NULL);
}
