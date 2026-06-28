/* SPDX-License-Identifier: MIT */
#include "ast.h"
#include "arena.h"

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

Node *node_decl(char *name, Node *init, SourceLoc loc)
{
    Node *n = new_node(ND_DECL, loc);
    n->name = name;
    n->init = init;
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

Function *func_new(char *name, Node *body)
{
    Function *f = arena_alloc_zeroed(sizeof(Function));
    f->name = name;
    f->body = body;
    return f;
}
