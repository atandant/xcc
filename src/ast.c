/* SPDX-License-Identifier: MIT */
#include "ast.h"
#include "arena.h"
#include "diag.h"

#include <limits.h>

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

Node *node_cast(Type *ty, Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_CAST, loc);
    n->cast_ty = ty;
    n->operand = o;
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

Node *node_decl(char *name, Type *ty, Node *init, SourceLoc loc)
{
    Node *n = new_node(ND_DECL, loc);
    n->name = name;
    n->ty = ty;
    n->init = init;
    return n;
}

Node *node_call(char *name, NodeList *args, SourceLoc loc)
{
    Node *n = new_node(ND_CALL, loc);
    n->name = name;
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

Node *node_for(Node *init, Node *cond, Node *step, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_FOR, loc);
    n->init = init;
    n->cond = cond;
    n->step = step;
    n->then_body = body;
    return n;
}

Node *node_block(Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_BLOCK, loc);
    n->body = body;
    return n;
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

    /* Build the full function type; array parameters decay to pointers. */
    Type **ptypes = NULL;
    if (pc->count > 0) {
        ptypes = arena_alloc(sizeof(Type *) * pc->count);
        int i = 0;
        for (Param *p = pc->head; p; p = p->next, i++)
            ptypes[i] = type_decay(p->ty);
    }
    f->ty = type_func(ret_ty, ptypes, pc->count, pc->prototyped);

    return f;
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

Declarator *declarator_ptr(Declarator *d)
{
    d->nptr++;
    return d;
}

static int decl_add_suffix_dim(Declarator *d, long dim, SourceLoc loc)
{
    if (d->ndims_suffix >= MAX_DECL_DIMS) {
        diag_error_at(loc, "too many array dimensions");
        return 0;
    }
    d->dims_suffix[d->ndims_suffix++] = dim;
    return 1;
}

Declarator *declarator_suffix(Declarator *d, long dim)
{
    (void)decl_add_suffix_dim(d, dim, (SourceLoc){ 0, 0 });
    return d;
}

Declarator *declarator_add_dim(Declarator *d, long dim, int after_paren)
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

Declarator *declarator_paren_outer(Declarator *d, long dim)
{
    Declarator *wrap = declarator_empty();
    wrap->inner = d->inner;
    wrap->dims_paren_outer[0] = dim;
    wrap->ndims_paren_outer = 1;
    return wrap;
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

static Type *make_array_dim(Type *ty, long dim, SourceLoc loc)
{
    int count;
    int esz;

    if (dim == 0) {
        /* `[]` on a parameter decays to a pointer before allocation. */
        return type_array(ty, 0);
    }
    if (dim < 0) {
        diag_error_at(loc, "array size is negative");
        return ty;
    }
    if (dim > INT_MAX) {
        diag_error_at(loc, "array size is too large");
        return ty;
    }
    count = (int)dim;
    esz = type_size(ty);
    if (esz > 0 && count > INT_MAX / esz) {
        diag_error_at(loc, "array size overflows");
        return ty;
    }
    return type_array(ty, count);
}

static int declarator_has_suffix_arrays(const Declarator *d)
{
    if (!d)
        return 0;
    if (d->ndims_suffix > 0)
        return 1;
    return declarator_has_suffix_arrays(d->inner);
}

static Type *apply_decl_leaf(Type *base, const Declarator *d, SourceLoc loc)
{
    Type *ty = base;
    int i;

    for (i = 0; i < d->nptr; i++)
        ty = type_ptr(ty);
    for (i = d->ndims_suffix - 1; i >= 0; i--)
        ty = make_array_dim(ty, d->dims_suffix[i], loc);
    return ty;
}

Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc)
{
    Type *ty;
    int i;

    if (!d)
        return base;

    if (type_is_pointer(base) && declarator_has_suffix_arrays(d)) {
        diag_error_at(loc, "array declarator not allowed on pointer type");
        return base;
    }

    if (d->nptr > 0 && d->ndims_suffix > 0) {
        diag_error_at(loc, "array declarator not allowed on pointer type");
        return base;
    }

    if (d->inner && d->ndims_paren_outer > 0) {
        /* `int (*p)[3]`: outer `[]` binds to the base before the inner `*`. */
        ty = base;
        for (i = d->ndims_paren_outer - 1; i >= 0; i--)
            ty = make_array_dim(ty, d->dims_paren_outer[i], loc);
        return apply_decl_leaf(ty, d->inner, loc);
    }

    if (d->was_paren && d->inner)
        return apply_decl_leaf(base, d->inner, loc);

    return apply_decl_leaf(base, d, loc);
}
