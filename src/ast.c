#include "ast.h"
#include "arena.h"

#include <string.h>

static Node *new_node(NodeKind kind, SourceLoc loc)
{
    Node *n = arena_alloc(sizeof(Node));
    memset(n, 0, sizeof(Node));
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

Node *stmt_append(Node *head, Node *s)
{
    if (!head)
        return s;
    Node *p = head;
    while (p->next)
        p = p->next;
    p->next = s;
    return head;
}

Function *func_new(char *name, Node *body)
{
    Function *f = arena_alloc(sizeof(Function));
    f->name = name;
    f->body = body;
    f->stack_size = 0;
    return f;
}
