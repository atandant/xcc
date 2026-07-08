/* SPDX-License-Identifier: MIT */
#ifndef XCC_AST_H
#define XCC_AST_H

#include "token.h"
#include "type.h"

#define MAX_DECL_DIMS 16
#define XCC_MAX_CALL_ARGS 4096

typedef struct Node Node;

typedef struct Declarator Declarator;
struct Declarator {
    char *name;
    int nptr;
    int ndims_suffix;
    Node *dims_suffix[MAX_DECL_DIMS]; /* NULL = unsized `[]`; ND_NUM = bound */
    int ndims_paren_outer;
    Node *dims_paren_outer[MAX_DECL_DIMS];
    int was_paren;     /* `(` declarator `)` with no following `[]` yet */
    Declarator *inner; /* set when `[]` immediately follows `( declarator )` */
    Node *bit_width_expr; /* struct bit-field `: width`; NULL if ordinary member */
};

typedef enum {
    ND_NUM,        /* integer literal              */
    ND_VAR,        /* reference to a local         */
    ND_BINOP,      /* lhs <op> rhs                 */
    ND_NEG,        /* unary minus on operand       */
    ND_ADDR,       /* unary & (address-of operand) */
    ND_DEREF,      /* unary * (dereference operand)*/
    ND_CAST,       /* (type)operand                */
    ND_SIZEOF,     /* sizeof expr / sizeof(type)   */
    ND_ASSIGN,     /* lhs = rhs (lhs must be modifiable lvalue) */
    ND_RETURN,     /* return operand;              */
    ND_EXPR_STMT,  /* operand;                     */
    ND_DECL,       /* type-specifier declarator [= init]; */
    ND_INIT_LIST,  /* brace initializer: body chain of expr / nested lists */
    ND_CALL,       /* name(args...)                */
    ND_IF,         /* if (cond) then [else else]   */
    ND_WHILE,      /* while (cond) body            */
    ND_FOR,        /* for (init; cond; step) body  */
    ND_BLOCK,      /* { body }                     */
    ND_TYPEDEF,    /* typedef specifier declarator; */
    ND_MEMBER      /* lhs.name  (`->` desugars to (*p).name) */
} NodeKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_COMMA
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
    int is_hex_literal;  /* ND_NUM: 0x/0X constant (C89 3.1.5 typing) */
    int is_octal_literal; /* ND_NUM: 0-prefixed octal (C89 3.1.5 typing) */
    char *name;        /* ND_VAR, ND_DECL                           */
    int offset;        /* stack offset from frame pointer (sema)     */

    BinOp op;          /* ND_BINOP                                  */
    Node *lhs, *rhs;   /* ND_BINOP, ND_ASSIGN                       */

    Node *operand;     /* ND_NEG, ND_RETURN, ND_EXPR_STMT, ND_CAST  */
    Type *cast_ty;     /* ND_CAST: parsed target type               */
    Type *decl_spec;   /* ND_DECL: base specifier before declarator */
    Declarator *decl;  /* ND_DECL: parsed declarator (sema → ty)    */
    Node *init;        /* ND_DECL initializer, ND_FOR init (NULL ok)*/
    Node *cond;        /* ND_IF, ND_WHILE, ND_FOR (FOR cond NULL ok)*/
    Node *step;        /* ND_FOR step (may be NULL)                 */
    Node *then_body;   /* ND_IF, ND_WHILE, ND_FOR loop body         */
    Node *else_body;   /* ND_IF (may be NULL)                       */
    Node *body;        /* ND_BLOCK linked list of statements        */

    Node *args;        /* ND_CALL argument list (chained via next)  */
    int nargs;         /* ND_CALL argument count                    */
    Type *func_ty;     /* ND_CALL: resolved callee TY_FUNC (sema)   */

    int member_index;  /* ND_MEMBER: index into struct Type.members */
};

/* A function parameter. `name` is NULL for an unnamed prototype parameter. */
typedef struct Param Param;
struct Param {
    char *name;
    Type *ty;          /* parameter type (sema)                     */
    Type *decl_spec;   /* parsed specifier when declarator pending  */
    Declarator *decl;  /* parsed declarator (sema → ty)             */
    int offset;        /* stack offset from frame pointer (sema)     */
    Param *next;
};

/* Result of parsing a parenthesized parameter list. `prototyped` is 0 for a
 * bare `()` (unspecified args, C89) and 1 for `(void)` or a real list. */
typedef struct {
    Param *head;
    int count;
    int prototyped;
} ParamClause;

typedef struct {
    int offset;
    int address_taken;
} FrameLocal;

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
    int locals_size;   /* named locals + spilled reg-param homes (sema) */
    FrameLocal *frame_locals;
    int nframe_locals;
    Function *next;    /* next function in the translation unit       */
};

/* File-scope typedef collected before functions (parser → sema). */
typedef struct TypedefDecl TypedefDecl;
struct TypedefDecl {
    Type *spec;
    Declarator *decl;
    SourceLoc loc;
    TypedefDecl *next;
};

/* One field in a struct declaration list (parser → struct_tag_define). */
typedef struct StructField StructField;
struct StructField {
    Type *spec;
    Declarator *decl;
    SourceLoc loc;
    StructField *next;
};

extern TypedefDecl *g_typedef_decls;

Node *node_num(long v, SourceLoc loc);
Node *node_var(char *name, SourceLoc loc);
Node *node_binop(BinOp op, Node *l, Node *r, SourceLoc loc);
Node *node_neg(Node *o, SourceLoc loc);
Node *node_addr(Node *o, SourceLoc loc);
Node *node_deref(Node *o, SourceLoc loc);
Node *node_member(Node *base, char *name, SourceLoc loc);
Node *node_cast(Type *ty, Node *o, SourceLoc loc);
Node *node_sizeof_expr(Node *o, SourceLoc loc);
Node *node_sizeof_type(Type *ty, SourceLoc loc);
Node *node_assign(Node *l, Node *r, SourceLoc loc);
Node *node_return(Node *o, SourceLoc loc);
Node *node_expr_stmt(Node *o, SourceLoc loc);
Node *node_decl(char *name, Type *spec_ty, Declarator *decl, Node *init,
                SourceLoc loc);
Node *node_init_list(Node *items, SourceLoc loc);
Node *init_list_append(Node *head, Node *item);
Node *node_call(char *name, NodeList *args, SourceLoc loc);
Node *node_if(Node *cond, Node *then_body, Node *else_body, SourceLoc loc);
Node *node_while(Node *cond, Node *body, SourceLoc loc);
Node *node_for(Node *init, Node *cond, Node *step, Node *body, SourceLoc loc);
Node *node_block(Node *body, SourceLoc loc);
Node *node_typedef(Type *spec, Declarator *decl, SourceLoc loc);
TypedefDecl *typedef_decl_new(Type *spec, Declarator *decl, SourceLoc loc);
TypedefDecl *typedef_decl_append(TypedefDecl *list, Type *spec, Declarator *decl,
                                 SourceLoc loc);
StructField *struct_field_append(StructField *list, Type *spec, Declarator *decl,
                                 SourceLoc loc);
StructField *struct_field_append_bit(StructField *list, Type *spec, Declarator *decl,
                                     Node *bit_width, SourceLoc loc);
Member *struct_fields_to_members(StructField *fields, int *out_n, SourceLoc loc);
NodeList *stmt_list_new(void);
NodeList *stmt_list_append(NodeList *list, Node *s);
Node *stmt_list_head(NodeList *list);

Param *param_append(Param *list, Type *ty, char *name);
Param *param_append_decl(Param *list, Type *spec_ty, Declarator *decl,
                         char *name);
ParamClause *param_clause(Param *head, int prototyped);
Function *func_new(char *name, ParamClause *pc, Type *ret_ty,
                   int is_definition, Node *body, SourceLoc loc);
Function *func_rebuild_type(Function *fn);
Function *func_append(Function *list, Function *f);

Declarator *declarator_empty(void);
Declarator *declarator_ident(char *name);
Declarator *declarator_bitfield(Declarator *d, Node *width);
Declarator *declarator_ptr(Declarator *d);
Declarator *declarator_add_dim(Declarator *d, Node *dim, int after_paren);
Declarator *declarator_paren_group(Declarator *d);
Declarator *declarator_paren_outer(Declarator *d, Node *dim);
int declarator_was_paren(const Declarator *d);
char *declarator_name(const Declarator *d);

typedef int (*DeclDimEvalFn)(Node *expr, long *out, SourceLoc loc, void *ctx);
Type *type_apply_declarator_cb(Type *base, Declarator *d, SourceLoc loc,
                               DeclDimEvalFn eval, void *ctx);
Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc);

#endif /* XCC_AST_H */
