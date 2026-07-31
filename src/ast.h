/* SPDX-License-Identifier: MIT */
#ifndef XCC_AST_H
#define XCC_AST_H

#include "token.h"
#include "type.h"

#define XCC_MAX_CALL_ARGS 4096

typedef struct Node Node;

typedef struct {
    unsigned char *data;
    int len;            /* decoded bytes, excluding the trailing null */
} StringToken;

typedef enum {
    STORAGE_NONE,
    STORAGE_EXTERN,
    STORAGE_STATIC,
    STORAGE_TYPEDEF,
    STORAGE_AUTO,
    STORAGE_REGISTER,
} StorageClass;

typedef enum {
    LINKAGE_NONE,
    LINKAGE_EXTERNAL,
    LINKAGE_INTERNAL,
} Linkage;

typedef enum {
    OBJECT_DECLARATION,
    OBJECT_TENTATIVE,
    OBJECT_DEFINITION,
} ObjectDeclKind;

typedef enum {
    TYPE_SPEC_VOID,
    TYPE_SPEC_CHAR,
    TYPE_SPEC_SHORT,
    TYPE_SPEC_INT,
    TYPE_SPEC_LONG,
    TYPE_SPEC_SIGNED,
    TYPE_SPEC_UNSIGNED,
} TypeSpecKind;

typedef struct DeclSpec DeclSpec;
struct DeclSpec {
    StorageClass storage;
    Type *named_type;
    int nconst;
    int nvoid;
    int nchar;
    int nshort;
    int nint;
    int nlong;
    int nsigned;
    int nunsigned;
};

typedef struct Declarator Declarator;
typedef struct ParamClause ParamClause;
typedef enum {
    DECL_IDENT,
    DECL_PTR,
    DECL_ARRAY,
    DECL_FUNC
} DeclaratorKind;

struct Declarator {
    DeclaratorKind kind;
    char *name;
    Declarator *inner;
    unsigned qualifiers; /* DECL_PTR: qualifiers following this '*' */
    Node *array_dim;   /* DECL_ARRAY: NULL means unsized `[]` */
    ParamClause *func_params; /* DECL_FUNC parameter clause */
    Node *bit_width_expr; /* struct bit-field `: width`; NULL if ordinary member */
};

typedef enum {
    ND_NUM,        /* integer literal              */
    ND_STRING,     /* ordinary character string literal */
    ND_VAR,        /* reference to a local         */
    ND_BINOP,      /* lhs <op> rhs                 */
    ND_POS,        /* unary plus on operand        */
    ND_NEG,        /* unary minus on operand       */
    ND_BITNOT,     /* bitwise complement operand   */
    ND_PREINC,     /* ++operand (prefix)           */
    ND_PREDEC,     /* --operand (prefix)           */
    ND_POSTINC,    /* operand++ (postfix)          */
    ND_POSTDEC,    /* operand-- (postfix)          */
    ND_NOT,        /* logical !operand             */
    ND_LOGAND,     /* lhs && rhs (short-circuit)   */
    ND_LOGOR,      /* lhs || rhs (short-circuit)   */
    ND_COND,       /* cond ? then_expr : else_expr */
    ND_ADDR,       /* unary & (address-of operand) */
    ND_DEREF,      /* unary * (dereference operand)*/
    ND_CAST,       /* (type)operand                */
    ND_SIZEOF,     /* sizeof expr / sizeof(type)   */
    ND_ASSIGN,     /* lhs = rhs (lhs must be modifiable lvalue) */
    ND_RETURN,     /* return operand;              */
    ND_EXPR_STMT,  /* operand;                     */
    ND_DECL,       /* type-specifier declarator [= init]; */
    ND_INIT_LIST,  /* brace initializer: body chain of expr / nested lists */
    ND_CALL,       /* callee(args...)              */
    ND_IF,         /* if (cond) then [else else]   */
    ND_WHILE,      /* while (cond) body            */
    ND_DO_WHILE,   /* do body while (cond)         */
    ND_FOR,        /* for (init; cond; step) body  */
    ND_SWITCH,     /* switch (cond) body            */
    ND_CASE,       /* case constant-expression: stmt */
    ND_DEFAULT,    /* default: stmt                 */
    ND_LABEL,      /* name: stmt                    */
    ND_GOTO,       /* goto name;                    */
    ND_BREAK,      /* break innermost loop/switch   */
    ND_CONTINUE,   /* continue innermost loop       */
    ND_BLOCK,      /* { body }                     */
    ND_TYPEDEF,    /* typedef specifier declarator; */
    ND_MEMBER      /* lhs.name  (`->` desugars to (*p).name) */
} NodeKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_SHL, OP_SHR,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_BITAND, OP_BITXOR, OP_BITOR,
    OP_COMMA
} BinOp;

typedef enum {
    VAR_STORAGE_NONE,
    VAR_STORAGE_LOCAL,
    VAR_STORAGE_GLOBAL,
    VAR_STORAGE_FUNCTION,
} VarStorage;

typedef struct Node Node;
typedef struct NodeList NodeList;
struct Node {
    NodeKind kind;
    SourceLoc loc;
    Node *next;        /* next statement in a list                  */

    Type *ty;          /* expression type (filled by sema)          */
    int is_lvalue;     /* 1 if this expression is an lvalue (sema)  */
    int var_decay;     /* ND_VAR: array decay address (sema); legacy alias */

    long val;          /* ND_NUM                                    */
    int has_long_suffix; /* ND_NUM: literal had an L/l suffix (C89 3.1.5) */
    int has_unsigned_suffix; /* ND_NUM: literal had a U/u suffix */
    int is_hex_literal;  /* ND_NUM: 0x/0X constant (C89 3.1.5 typing) */
    int is_octal_literal; /* ND_NUM: 0-prefixed octal (C89 3.1.5 typing) */
    int is_char_constant; /* ND_NUM: ordinary character constant (type int) */
    unsigned char *string_data; /* ND_STRING: decoded bytes, no final null */
    int string_len;      /* ND_STRING: decoded byte count             */
    char *string_label;  /* ND_STRING: private assembler symbol       */
    Node *string_next;   /* ND_STRING: translation-unit literal list  */
    char *name;        /* ND_VAR, ND_DECL; ND_CALL direct callee name */
    char *symbol_name; /* ND_VAR: assembler name when different       */
    Node *callee;      /* ND_CALL: callee expression (sema)         */
    int call_direct;   /* ND_CALL: 1 → emit direct call by name     */
    int func_decay;    /* ND_VAR: 1 → function/array address escape  */
    VarStorage storage;/* ND_VAR: resolved object/function storage   */
    int offset;        /* stack offset from frame pointer (sema)     */

    BinOp op;          /* ND_BINOP; compound ND_ASSIGN operation    */
    int is_compound_assign; /* ND_ASSIGN: lhs op= rhs                */
    Type *op_ty;       /* ND_ASSIGN: promoted compound operation type */
    Node *lhs, *rhs;   /* ND_BINOP, ND_ASSIGN                       */

    Node *operand;     /* unary expressions, ND_RETURN, ND_EXPR_STMT,
                        * ND_CAST, ND_CASE constant expression       */
    Type *cast_ty;     /* ND_CAST: parsed target type               */
    StorageClass decl_storage; /* ND_DECL: declared storage class     */
    Type *decl_spec;   /* ND_DECL: base specifier before declarator */
    Declarator *decl;  /* ND_DECL: parsed declarator (sema → ty)    */
    Node *init;        /* ND_DECL initializer, ND_FOR init (NULL ok)*/
    Node *cond;        /* ND_IF, ND_WHILE, ND_DO_WHILE, ND_FOR, ND_SWITCH */
    Node *then_expr;   /* ND_COND selected when cond is nonzero      */
    Node *else_expr;   /* ND_COND selected when cond is zero         */
    Node *step;        /* ND_FOR step (may be NULL)                 */
    Node *then_body;   /* ND_IF/loops/switch body, ND_CASE/DEFAULT stmt */
    Node *else_body;   /* ND_IF (may be NULL)                       */
    Node *body;        /* ND_BLOCK linked list of statements        */

    Node *cases;       /* ND_SWITCH: source-ordered ND_CASE list    */
    Node *default_case;/* ND_SWITCH: ND_DEFAULT, if present         */
    Node *case_next;   /* ND_CASE: next case in enclosing switch    */
    long case_val;     /* ND_CASE: converted controlling-type value */
    int label;         /* ND_CASE/ND_DEFAULT/ND_LABEL lowering label */

    Node *goto_target; /* ND_GOTO: resolved function-scope label    */
    Node *label_next;  /* ND_LABEL: next label in sema lookup list  */

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
    StorageClass storage; /* only STORAGE_REGISTER is valid          */
    int offset;        /* stack offset from frame pointer (sema)     */
    int abi_gpr_start; /* first SysV arg-reg slot (0=rdi..5=r9); -1 stack */
    int abi_ngpr;      /* consecutive GPR slots (0 = stack-only arg)   */
    int abi_stack_bytes; /* bytes on caller stack when abi_ngpr == 0    */
    Param *next;
};

/* Result of parsing a parenthesized parameter list. `prototyped` is 0 for a
 * bare `()` (unspecified args, C89) and 1 for `(void)` or a real list. */
struct ParamClause {
    Param *head;
    int count;
    int prototyped;
};

typedef struct {
    int offset;
    int size;
    int promotable_scalar;
    int address_taken;
} FrameLocal;

typedef struct Function Function;
struct Function {
    char *name;
    SourceLoc loc;
    StorageClass storage;
    Linkage linkage;
    Param *params;
    int nparams;
    int prototyped;    /* 0 = bare () unspecified args; 1 = checked  */
    Type *ret_ty;      /* function return type (source of truth)     */
    Type *ty;          /* full TY_FUNC type for this function         */
    int is_definition; /* 1 if it has a body; 0 if just a prototype   */
    Node *body;        /* linked list of statements (NULL for decl)  */
    int has_goto;      /* skip lexical AST propagation for arbitrary CFG */
    int locals_size;   /* named locals + spilled reg-param homes (sema) */
    int abi_ret_sret;  /* 1: return >16 bytes via hidden pointer in RDI */
    int abi_sret_offset; /* frame slot holding incoming sret pointer    */
    int abi_call_scratch; /* ephemeral stack for orphan sret call results */
    FrameLocal *frame_locals;
    int nframe_locals;
    Function *next;    /* next function in the translation unit       */
};

typedef struct GlobalObject GlobalObject;
typedef struct StaticReloc StaticReloc;
struct StaticReloc {
    int offset;
    int width;
    char *symbol;
    long addend;
    StaticReloc *next;
};

struct GlobalObject {
    char *name;
    char *source_name;
    SourceLoc loc;
    StorageClass storage;
    Linkage linkage;
    ObjectDeclKind decl_kind;
    Type *decl_spec;
    Declarator *decl;
    Type *ty;
    Node *init;
    unsigned char *init_data;
    int init_size;
    StaticReloc *relocs;
    int emit;
    GlobalObject *next_static;
};

typedef enum {
    EXT_FUNCTION,
    EXT_OBJECT,
} ExternalKind;

typedef struct ExternalDecl ExternalDecl;
struct ExternalDecl {
    ExternalKind kind;
    Function *function;
    GlobalObject *object;
    ExternalDecl *next;
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

/* One enumerator in an enum declaration list (parser → enum_tag_define).
 * `value` is the optional `= constant-expression`; NULL means implicit. */
typedef struct Enumerator Enumerator;
struct Enumerator {
    char *name;
    Node *value;
    SourceLoc loc;
    Enumerator *next;
};

extern TypedefDecl *g_typedef_decls;

Node *node_num(long v, SourceLoc loc);
DeclSpec *declspec_new(void);
DeclSpec *declspec_add_storage(DeclSpec *spec, StorageClass storage,
                               SourceLoc loc);
DeclSpec *declspec_add_type(DeclSpec *spec, Type *ty, SourceLoc loc);
DeclSpec *declspec_add_qualifier(DeclSpec *spec, unsigned qualifier,
                                 SourceLoc loc);
DeclSpec *declspec_add_builtin(DeclSpec *spec, TypeSpecKind kind,
                               SourceLoc loc);
Type *declspec_type(DeclSpec *spec, SourceLoc loc);
Node *node_string(StringToken *token, SourceLoc loc);
Node *node_string_append(Node *literal, StringToken *token);
Node *string_literals(void);
Node *node_var(char *name, SourceLoc loc);
Node *node_binop(BinOp op, Node *l, Node *r, SourceLoc loc);
Node *node_pos(Node *o, SourceLoc loc);
Node *node_neg(Node *o, SourceLoc loc);
Node *node_bitnot(Node *o, SourceLoc loc);
Node *node_preinc(Node *o, SourceLoc loc);
Node *node_predec(Node *o, SourceLoc loc);
Node *node_postinc(Node *o, SourceLoc loc);
Node *node_postdec(Node *o, SourceLoc loc);
Node *node_not(Node *o, SourceLoc loc);
Node *node_logand(Node *l, Node *r, SourceLoc loc);
Node *node_logor(Node *l, Node *r, SourceLoc loc);
Node *node_cond(Node *cond, Node *then_expr, Node *else_expr, SourceLoc loc);
Node *node_addr(Node *o, SourceLoc loc);
Node *node_deref(Node *o, SourceLoc loc);
Node *node_member(Node *base, char *name, SourceLoc loc);
Node *node_cast(Type *ty, Node *o, SourceLoc loc);
Node *node_sizeof_expr(Node *o, SourceLoc loc);
Node *node_sizeof_type(Type *ty, SourceLoc loc);
Node *node_assign(Node *l, Node *r, SourceLoc loc);
Node *node_compound_assign(BinOp op, Node *l, Node *r, SourceLoc loc);
Node *node_return(Node *o, SourceLoc loc);
Node *node_expr_stmt(Node *o, SourceLoc loc);
Node *node_decl(char *name, Type *spec_ty, Declarator *decl,
                StorageClass storage, Node *init, SourceLoc loc);
Node *node_init_list(Node *items, SourceLoc loc);
Node *init_list_append(Node *head, Node *item);
Node *node_call(Node *callee, NodeList *args, SourceLoc loc);
Node *node_if(Node *cond, Node *then_body, Node *else_body, SourceLoc loc);
Node *node_while(Node *cond, Node *body, SourceLoc loc);
Node *node_do_while(Node *body, Node *cond, SourceLoc loc);
Node *node_for(Node *init, Node *cond, Node *step, Node *body, SourceLoc loc);
Node *node_switch(Node *cond, Node *body, SourceLoc loc);
Node *node_case(Node *expr, Node *stmt, SourceLoc loc);
Node *node_default(Node *stmt, SourceLoc loc);
Node *node_label(char *name, Node *stmt, SourceLoc loc);
Node *node_goto(char *name, SourceLoc loc);
Node *node_break(SourceLoc loc);
Node *node_continue(SourceLoc loc);
Node *node_block(Node *body, SourceLoc loc);
Node *node_typedef(Type *spec, Declarator *decl, SourceLoc loc);
TypedefDecl *typedef_decl_new(Type *spec, Declarator *decl, SourceLoc loc);
TypedefDecl *typedef_decl_append(TypedefDecl *list, Type *spec, Declarator *decl,
                                 SourceLoc loc);
StructField *struct_field_append(StructField *list, Type *spec, Declarator *decl,
                                 SourceLoc loc);
StructField *struct_field_append_bit(StructField *list, Type *spec, Declarator *decl,
                                     Node *bit_width, SourceLoc loc);
Enumerator *enumerator_new(char *name, Node *value, SourceLoc loc);
Enumerator *enumerator_append(Enumerator *list, Enumerator *e);
Member *struct_fields_to_members(StructField *fields, int *out_n, SourceLoc loc);
NodeList *stmt_list_new(void);
NodeList *stmt_list_append(NodeList *list, Node *s);
Node *stmt_list_head(NodeList *list);

Param *param_append(Param *list, Type *ty, char *name);
Param *param_append_decl(Param *list, Type *spec_ty, Declarator *decl,
                         char *name);
ParamClause *param_clause(Param *head, int prototyped);
Function *func_new(char *name, ParamClause *pc, Type *ret_ty,
                   StorageClass storage, int is_definition, Node *body,
                   SourceLoc loc);
Function *func_new_decl(Type *spec, Declarator *decl, StorageClass storage,
                        int is_definition, Node *body, SourceLoc loc);
Function *func_rebuild_type(Function *fn);
Function *func_append(Function *list, Function *f);
ExternalDecl *external_function(Function *fn);
ExternalDecl *external_declaration(Type *spec, Declarator *decl,
                                   StorageClass storage, Node *init,
                                   SourceLoc loc);
ExternalDecl *external_append(ExternalDecl *list, ExternalDecl *external);
Function *external_functions(ExternalDecl *list);

Declarator *declarator_empty(void);
Declarator *declarator_ident(char *name);
Declarator *declarator_bitfield(Declarator *d, Node *width);
Declarator *declarator_ptr(Declarator *d, unsigned qualifiers);
Declarator *declarator_add_dim(Declarator *d, Node *dim, int after_paren);
Declarator *declarator_paren_group(Declarator *d);
Declarator *declarator_paren_outer(Declarator *d, Node *dim);
Declarator *declarator_func(Declarator *d, ParamClause *pc);
int declarator_was_paren(const Declarator *d);
char *declarator_name(const Declarator *d);
ParamClause *declarator_function_params(const Declarator *d);

typedef int (*DeclDimEvalFn)(Node *expr, long *out, SourceLoc loc, void *ctx);
Type *type_apply_declarator_cb(Type *base, Declarator *d, SourceLoc loc,
                               DeclDimEvalFn eval, void *ctx);
Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc);

#endif /* XCC_AST_H */
