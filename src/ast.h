/* SPDX-License-Identifier: MIT */
#ifndef XCC_AST_H
#define XCC_AST_H

#include "token.h"
#include "type.h"

#define MAX_DECL_DIMS 16
#define XCC_MAX_CALL_ARGS 4096

typedef struct {
    char *name;
    int ndims;
    long dims[MAX_DECL_DIMS];
} Declarator;

typedef enum {
    ND_NUM,        /* integer literal              */
    ND_VAR,        /* reference to a local         */
    ND_BINOP,      /* lhs <op> rhs                 */
    ND_NEG,        /* unary minus on operand       */
    ND_ADDR,       /* unary & (address-of operand) */
    ND_DEREF,      /* unary * (dereference operand)*/
    ND_CAST,       /* (type)operand                */
    ND_ASSIGN,     /* lhs = rhs (lhs must be modifiable lvalue) */
    ND_RETURN,     /* return operand;              */
    ND_EXPR_STMT,  /* operand;                     */
    ND_DECL,       /* type-specifier declarator [= init]; */
    ND_CALL,       /* name(args...)                */
    ND_IF,         /* if (cond) then [else else]   */
    ND_WHILE,      /* while (cond) body            */
    ND_FOR,        /* for (init; cond; step) body  */
    ND_BLOCK       /* { body }                     */
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

    Type *ty;          /* expression type (filled by sema)          */
    int is_lvalue;     /* 1 if this expression is an lvalue (sema)  */
    int var_decay;     /* ND_VAR: 1 → address of array object (sema) */

    long val;          /* ND_NUM                                    */
    int has_long_suffix; /* ND_NUM: literal had an L/l suffix (C89 3.1.5) */
    char *name;        /* ND_VAR, ND_DECL                           */
    int offset;        /* stack offset from %rbp (filled by sema)   */

    BinOp op;          /* ND_BINOP                                  */
    Node *lhs, *rhs;   /* ND_BINOP, ND_ASSIGN                       */

    Node *operand;     /* ND_NEG, ND_RETURN, ND_EXPR_STMT, ND_CAST  */
    Type *cast_ty;     /* ND_CAST: parsed target type               */
    Node *init;        /* ND_DECL initializer, ND_FOR init (NULL ok)*/
    Node *cond;        /* ND_IF, ND_WHILE, ND_FOR (FOR cond NULL ok)*/
    Node *step;        /* ND_FOR step (may be NULL)                 */
    Node *then_body;   /* ND_IF, ND_WHILE, ND_FOR loop body         */
    Node *else_body;   /* ND_IF (may be NULL)                       */
    Node *body;        /* ND_BLOCK linked list of statements        */

    Node *args;        /* ND_CALL argument list (chained via next)  */
    int nargs;         /* ND_CALL argument count                    */
    Type *func_ty;     /* ND_CALL: resolved callee TY_FUNC (sema)   */
};

/* A function parameter. `name` is NULL for an unnamed prototype parameter. */
typedef struct Param Param;
struct Param {
    char *name;
    Type *ty;          /* parameter type                            */
    int offset;        /* stack offset from %rbp (filled by sema)   */
    Param *next;
};

/* Result of parsing a parenthesized parameter list. `prototyped` is 0 for a
 * bare `()` (unspecified args, C89) and 1 for `(void)` or a real list. */
typedef struct {
    Param *head;
    int count;
    int prototyped;
} ParamClause;

typedef struct Function Function;
struct Function {
    char *name;
    SourceLoc loc;
    Param *params;
    int nparams;
    int prototyped;    /* 0 = bare () unspecified args; 1 = checked  */
    Type *ret_ty;      /* function return type (source of truth)     */
    Type *ty;          /* full TY_FUNC type for this function         */
    int is_definition; /* 1 if it has a body; 0 if just a prototype   */
    Node *body;        /* linked list of statements (NULL for decl)  */
    int stack_size;    /* total frame bytes, 16-aligned (sema)       */
    int locals_size;   /* locals + spilled reg params (sema)         */
    Function *next;    /* next function in the translation unit       */
};

Node *node_num(long v, SourceLoc loc);
Node *node_var(char *name, SourceLoc loc);
Node *node_binop(BinOp op, Node *l, Node *r, SourceLoc loc);
Node *node_neg(Node *o, SourceLoc loc);
Node *node_addr(Node *o, SourceLoc loc);
Node *node_deref(Node *o, SourceLoc loc);
Node *node_cast(Type *ty, Node *o, SourceLoc loc);
Node *node_assign(Node *l, Node *r, SourceLoc loc);
Node *node_return(Node *o, SourceLoc loc);
Node *node_expr_stmt(Node *o, SourceLoc loc);
Node *node_decl(char *name, Type *ty, Node *init, SourceLoc loc);
Node *node_call(char *name, NodeList *args, SourceLoc loc);
Node *node_if(Node *cond, Node *then_body, Node *else_body, SourceLoc loc);
Node *node_while(Node *cond, Node *body, SourceLoc loc);
Node *node_for(Node *init, Node *cond, Node *step, Node *body, SourceLoc loc);
Node *node_block(Node *body, SourceLoc loc);
NodeList *stmt_list_new(void);
NodeList *stmt_list_append(NodeList *list, Node *s);
Node *stmt_list_head(NodeList *list);

Param *param_append(Param *list, Type *ty, char *name);
ParamClause *param_clause(Param *head, int prototyped);
Function *func_new(char *name, ParamClause *pc, Type *ret_ty,
                   int is_definition, Node *body, SourceLoc loc);
Function *func_append(Function *list, Function *f);
Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc);

#endif /* XCC_AST_H */
