/* SPDX-License-Identifier: MIT */
#include "ast.h"
#include "arena.h"
#include "diag.h"

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

Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc)
{
    Type *ty = base;
    int i;

    if (type_is_pointer(base) && d->ndims > 0) {
        diag_error_at(loc, "array declarator not allowed on pointer type");
        return base;
    }

    /* Build the type inside-out: the rightmost dimension is the innermost
     * (element) array, so `int a[2][3]` is array[2] of array[3] of int. */
    for (i = d->ndims - 1; i >= 0; i--)
        ty = type_array(ty, (int)d->dims[i]);
    return ty;
}
