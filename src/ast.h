/* SPDX-License-Identifier: MIT */
#ifndef XCC_AST_H
#define XCC_AST_H

#include "token.h"

typedef enum {
    ND_NUM,        /* integer literal              */
    ND_VAR,        /* reference to a local         */
    ND_BINOP,      /* lhs <op> rhs                 */
    ND_NEG,        /* unary minus on operand       */
    ND_ASSIGN,     /* lhs = rhs   (lhs must be VAR)*/
    ND_RETURN,     /* return operand;              */
    ND_EXPR_STMT,  /* operand;                     */
    ND_DECL        /* int name [= init];           */
} NodeKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
} BinOp;

typedef struct Node Node;
typedef struct NodeList NodeList;
struct Node {
    NodeKind kind;
    SourceLoc loc;
    Node *next;        /* next statement in a list                  */

    long val;          /* ND_NUM                                    */
    char *name;        /* ND_VAR, ND_DECL                           */
    int offset;        /* stack offset from %rbp (filled by sema)   */

    BinOp op;          /* ND_BINOP                                  */
    Node *lhs, *rhs;   /* ND_BINOP, ND_ASSIGN                       */

    Node *operand;     /* ND_NEG, ND_RETURN, ND_EXPR_STMT           */
    Node *init;        /* ND_DECL initializer (may be NULL)         */
};

typedef struct {
    char *name;
    Node *body;        /* linked list of statements                 */
    int stack_size;    /* bytes reserved for locals (filled by sema)*/
} Function;

Node *node_num(long v, SourceLoc loc);
Node *node_var(char *name, SourceLoc loc);
Node *node_binop(BinOp op, Node *l, Node *r, SourceLoc loc);
Node *node_neg(Node *o, SourceLoc loc);
Node *node_assign(Node *l, Node *r, SourceLoc loc);
Node *node_return(Node *o, SourceLoc loc);
Node *node_expr_stmt(Node *o, SourceLoc loc);
Node *node_decl(char *name, Node *init, SourceLoc loc);
NodeList *stmt_list_new(void);
NodeList *stmt_list_append(NodeList *list, Node *s);
Node *stmt_list_head(NodeList *list);
Function *func_new(char *name, Node *body);

#endif /* XCC_AST_H */
