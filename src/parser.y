/* SPDX-License-Identifier: MIT */
%{
#include <stdio.h>
#include <stdlib.h>

#include "token.h"
#include "ast.h"
#include "diag.h"
#include "arena.h"
#include "sema_typedef.h"
#include "sema_struct.h"
#include "sema_enum.h"

extern int yylex(void);
void yyerror(const char *msg);

ExternalDecl *g_program = NULL;

#define LOC(L) ((SourceLoc){ (L).file, (L).first_line, (L).first_column })

static Type *declspec_without_storage(DeclSpec *spec, SourceLoc loc,
                                      const char *context)
{
    if (spec->storage != STORAGE_NONE)
        diag_error_at(loc, "storage class specified for %s", context);
    return declspec_type(spec, loc);
}

static Type *parameter_declspec_type(DeclSpec *spec, SourceLoc loc)
{
    if (spec->storage != STORAGE_NONE &&
        spec->storage != STORAGE_REGISTER)
        diag_error_at(loc, "invalid storage class for parameter");
    return declspec_type(spec, loc);
}

/* The declaration specifiers are shared by every init-declarator in a local
   comma-separated declaration. The parser is non-reentrant, and an
   initializer cannot contain another declaration, so one active context is
   sufficient. */
static DeclSpec *local_declspec;

void parser_reset(void)
{
    g_program = NULL;
    local_declspec = NULL;
}
%}

%locations
%define api.location.type {XccLocation}
%code top {
#define YYLLOC_DEFAULT(Current, Rhs, N)                                  \
    do {                                                                 \
        if (N) {                                                         \
            (Current).file = YYRHSLOC(Rhs, 1).file;                      \
            (Current).first_line = YYRHSLOC(Rhs, 1).first_line;          \
            (Current).first_column = YYRHSLOC(Rhs, 1).first_column;      \
            (Current).last_line = YYRHSLOC(Rhs, N).last_line;            \
            (Current).last_column = YYRHSLOC(Rhs, N).last_column;        \
        } else {                                                         \
            (Current) = YYRHSLOC(Rhs, 0);                                \
        }                                                                \
    } while (0)
}
%define parse.error verbose
/* Storage classes and qualifiers next to a typedef name are intentionally
   shifted into declaration specifiers rather than reduced as an implicit-int
   declaration whose declarator happens to use that spelling. */
%expect 29

%code requires {
    #include "ast.h"
    #include "token.h"
    #include "type.h"
}

%union {
    struct {
        long val;
        int is_long;
        int is_unsigned;
        int is_hex;
        int is_octal;
        int is_char;
        long double float_val;
        int is_floating;
        int is_float_suffix;
        int is_long_double_suffix;
    } num;
    char *str;
    StringToken *string;
    Node *node;
    NodeList *list;
    Param *param;
    ParamClause *pclause;
    Function *func;
    ExternalDecl *external;
    Type *type;
    DeclSpec *declspec;
    Declarator *decl;
    StructField *fields;
    Enumerator *enumr;
    int scope;
}

%token <num> NUM
%token <str> IDENT TYPEDEF_NAME
%token <string> STRING
%token INT CHAR SHORT LONG FLOAT DOUBLE VOID UNSIGNED SIGNED CONST VOLATILE RETURN IF ELSE WHILE DO FOR SWITCH CASE DEFAULT GOTO BREAK CONTINUE SIZEOF TYPEDEF EXTERN STATIC AUTO REGISTER STRUCT UNION ENUM
%token EQ NE LE GE ARROW LAND LOR SHL SHR INC DEC
%token MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN SUB_ASSIGN
%token SHL_ASSIGN SHR_ASSIGN AND_ASSIGN XOR_ASSIGN OR_ASSIGN
%token ELLIPSIS
%token BUILTIN_VA_START BUILTIN_VA_ARG BUILTIN_VA_END
%token BUILTIN_SETJMP BUILTIN_LONGJMP

%type <node> expr expr_opt arg_expr conditional_expr logical_or_expr
             logical_and_expr bitwise_or_expr bitwise_xor_expr bitwise_and_expr
             equality_expr relational_expr shift_expr additive_expr
             multiplicative_expr cast_expr unary_expr postfix_expr primary_expr
             string_literal stmt initializer init_list initializer_opt
             local_declaration local_init_declarator
%type <list> stmt_list arg_clause arg_list local_init_declarator_list
%type <param> param_list param
%type <pclause> param_clause
%type <external> toplevel
%type <type> cast_type struct_specifier union_specifier enum_specifier
%type <declspec> declaration_specifiers builtin_declaration_specifiers
                 named_declaration_specifiers
%type <fields> struct_declaration_list struct_declaration struct_declarator_list
               struct_decl_item
%type <enumr> enumerator_list enumerator
%type <str> struct_tag enum_tag member_name label_name
%type <decl> declarator direct_declarator abstract_declarator
             abstract_declarator_opt direct_abstract_declarator
%type <scope> param_scope_start block_scope_start function_body_scope_start
%type <scope> type_qualifier_list

%precedence IFX
%precedence ELSE
%right CONST VOLATILE

%destructor { if ($$) { typedef_leave_scope(); struct_tag_leave_scope(); } }
            param_scope_start block_scope_start function_body_scope_start

%%

program:
    %empty                   { }
  | program toplevel         { if ($2) g_program = external_append(g_program, $2); }
  | program struct_toplevel    { }
  ;

struct_toplevel:
    struct_specifier ';'       { (void)$1; }
  | union_specifier ';'        { (void)$1; }
  | enum_specifier ';'         { (void)$1; }
  ;

toplevel:
    declaration_specifiers declarator function_body_scope_start
        {
            ParamClause *pc = declarator_function_params($2);
            for (Param *p = pc ? pc->head : NULL; p; p = p->next)
                typedef_hide_name(p->name, LOC(@2));
        }
    '{' stmt_list '}'
        { (void)$3; typedef_leave_scope(); struct_tag_leave_scope();
          Type *ty = declspec_type($1, LOC(@1));
          if ($1->storage == STORAGE_TYPEDEF)
              diag_error_at(LOC(@1), "function definition declared 'typedef'");
          $$ = external_function(
              func_new_decl(ty, $2, $1->storage, 1, stmt_list_head($6),
                            LOC(@2))); }
  | declaration_specifiers declarator initializer_opt ';'
        {
            Type *ty = declspec_type($1, LOC(@1));
            if ($1->storage == STORAGE_TYPEDEF) {
                if ($3)
                    diag_error_at(LOC(@2), "typedef '%s' is initialized",
                                  declarator_name($2));
                typedef_declare(ty, $2, LOC(@1));
                g_typedef_decls = typedef_decl_append(g_typedef_decls, ty, $2,
                                                      LOC(@1));
                $$ = NULL;
            } else {
                $$ = external_declaration(ty, $2, $1->storage, $3, LOC(@2));
            }
        }
  ;

/* C89 declaration specifiers may appear in any order. Builtin specifiers are
   accumulated and validated as a set instead of enumerating spellings. */
declaration_specifiers:
    builtin_declaration_specifiers { $$ = $1; }
  | named_declaration_specifiers   { $$ = $1; }
  | CONST VOLATILE builtin_declaration_specifiers
        { $$ = declspec_add_qualifier($3, TQ_CONST, LOC(@1));
          $$ = declspec_add_qualifier($$, TQ_VOLATILE, LOC(@2)); }
  | VOLATILE CONST builtin_declaration_specifiers
        { $$ = declspec_add_qualifier($3, TQ_VOLATILE, LOC(@1));
          $$ = declspec_add_qualifier($$, TQ_CONST, LOC(@2)); }
  ;

builtin_declaration_specifiers:
    INT                 { $$ = declspec_add_builtin(NULL, TYPE_SPEC_INT, LOC(@1)); }
  | CHAR                { $$ = declspec_add_builtin(NULL, TYPE_SPEC_CHAR, LOC(@1)); }
  | SHORT               { $$ = declspec_add_builtin(NULL, TYPE_SPEC_SHORT, LOC(@1)); }
  | LONG                { $$ = declspec_add_builtin(NULL, TYPE_SPEC_LONG, LOC(@1)); }
  | VOID                { $$ = declspec_add_builtin(NULL, TYPE_SPEC_VOID, LOC(@1)); }
  | SIGNED              { $$ = declspec_add_builtin(NULL, TYPE_SPEC_SIGNED, LOC(@1)); }
  | UNSIGNED            { $$ = declspec_add_builtin(NULL, TYPE_SPEC_UNSIGNED, LOC(@1)); }
  | FLOAT               { $$ = declspec_add_builtin(NULL, TYPE_SPEC_FLOAT, LOC(@1)); }
  | DOUBLE              { $$ = declspec_add_builtin(NULL, TYPE_SPEC_DOUBLE, LOC(@1)); }
  | EXTERN              { $$ = declspec_add_storage(NULL, STORAGE_EXTERN, LOC(@1)); }
  | STATIC              { $$ = declspec_add_storage(NULL, STORAGE_STATIC, LOC(@1)); }
  | TYPEDEF             { $$ = declspec_add_storage(NULL, STORAGE_TYPEDEF, LOC(@1)); }
  | AUTO                { $$ = declspec_add_storage(NULL, STORAGE_AUTO, LOC(@1)); }
  | REGISTER            { $$ = declspec_add_storage(NULL, STORAGE_REGISTER, LOC(@1)); }
  | CONST               { $$ = declspec_add_qualifier(NULL, TQ_CONST, LOC(@1)); }
  | VOLATILE            { $$ = declspec_add_qualifier(NULL, TQ_VOLATILE, LOC(@1)); }
  | builtin_declaration_specifiers INT
        { $$ = declspec_add_builtin($1, TYPE_SPEC_INT, LOC(@2)); }
  | builtin_declaration_specifiers CHAR
        { $$ = declspec_add_builtin($1, TYPE_SPEC_CHAR, LOC(@2)); }
  | builtin_declaration_specifiers SHORT
        { $$ = declspec_add_builtin($1, TYPE_SPEC_SHORT, LOC(@2)); }
  | builtin_declaration_specifiers LONG
        { $$ = declspec_add_builtin($1, TYPE_SPEC_LONG, LOC(@2)); }
  | builtin_declaration_specifiers VOID
        { $$ = declspec_add_builtin($1, TYPE_SPEC_VOID, LOC(@2)); }
  | builtin_declaration_specifiers SIGNED
        { $$ = declspec_add_builtin($1, TYPE_SPEC_SIGNED, LOC(@2)); }
  | builtin_declaration_specifiers UNSIGNED
        { $$ = declspec_add_builtin($1, TYPE_SPEC_UNSIGNED, LOC(@2)); }
  | builtin_declaration_specifiers FLOAT
        { $$ = declspec_add_builtin($1, TYPE_SPEC_FLOAT, LOC(@2)); }
  | builtin_declaration_specifiers DOUBLE
        { $$ = declspec_add_builtin($1, TYPE_SPEC_DOUBLE, LOC(@2)); }
  | builtin_declaration_specifiers EXTERN
        { $$ = declspec_add_storage($1, STORAGE_EXTERN, LOC(@2)); }
  | builtin_declaration_specifiers STATIC
        { $$ = declspec_add_storage($1, STORAGE_STATIC, LOC(@2)); }
  | builtin_declaration_specifiers TYPEDEF
        { $$ = declspec_add_storage($1, STORAGE_TYPEDEF, LOC(@2)); }
  | builtin_declaration_specifiers AUTO
        { $$ = declspec_add_storage($1, STORAGE_AUTO, LOC(@2)); }
  | builtin_declaration_specifiers REGISTER
        { $$ = declspec_add_storage($1, STORAGE_REGISTER, LOC(@2)); }
  | builtin_declaration_specifiers CONST
        { $$ = declspec_add_qualifier($1, TQ_CONST, LOC(@2)); }
  | builtin_declaration_specifiers VOLATILE
        { $$ = declspec_add_qualifier($1, TQ_VOLATILE, LOC(@2)); }
  ;

/* A typedef name or tagged type is already a complete type specifier; only a
   single storage class may surround it. Keeping this non-recursive also
   preserves the typedef-name/declarator disambiguation used by the lexer. */
named_declaration_specifiers:
    TYPEDEF_NAME         { $$ = declspec_add_type(NULL, typedef_lookup($1), LOC(@1)); }
  | struct_specifier     { $$ = declspec_add_type(NULL, $1, LOC(@1)); }
  | union_specifier      { $$ = declspec_add_type(NULL, $1, LOC(@1)); }
  | enum_specifier       { $$ = declspec_add_type(NULL, $1, LOC(@1)); }
  | EXTERN TYPEDEF_NAME  { $$ = declspec_add_storage(NULL, STORAGE_EXTERN, LOC(@1));
                           $$ = declspec_add_type($$, typedef_lookup($2), LOC(@2)); }
  | STATIC TYPEDEF_NAME  { $$ = declspec_add_storage(NULL, STORAGE_STATIC, LOC(@1));
                           $$ = declspec_add_type($$, typedef_lookup($2), LOC(@2)); }
  | TYPEDEF TYPEDEF_NAME { $$ = declspec_add_storage(NULL, STORAGE_TYPEDEF, LOC(@1));
                           $$ = declspec_add_type($$, typedef_lookup($2), LOC(@2)); }
  | AUTO TYPEDEF_NAME    { $$ = declspec_add_storage(NULL, STORAGE_AUTO, LOC(@1));
                           $$ = declspec_add_type($$, typedef_lookup($2), LOC(@2)); }
  | REGISTER TYPEDEF_NAME { $$ = declspec_add_storage(NULL, STORAGE_REGISTER, LOC(@1));
                            $$ = declspec_add_type($$, typedef_lookup($2), LOC(@2)); }
  | TYPEDEF_NAME EXTERN  { $$ = declspec_add_type(NULL, typedef_lookup($1), LOC(@1));
                           $$ = declspec_add_storage($$, STORAGE_EXTERN, LOC(@2)); }
  | TYPEDEF_NAME STATIC  { $$ = declspec_add_type(NULL, typedef_lookup($1), LOC(@1));
                           $$ = declspec_add_storage($$, STORAGE_STATIC, LOC(@2)); }
  | TYPEDEF_NAME TYPEDEF { $$ = declspec_add_type(NULL, typedef_lookup($1), LOC(@1));
                           $$ = declspec_add_storage($$, STORAGE_TYPEDEF, LOC(@2)); }
  | TYPEDEF_NAME AUTO    { $$ = declspec_add_type(NULL, typedef_lookup($1), LOC(@1));
                           $$ = declspec_add_storage($$, STORAGE_AUTO, LOC(@2)); }
  | TYPEDEF_NAME REGISTER { $$ = declspec_add_type(NULL, typedef_lookup($1), LOC(@1));
                            $$ = declspec_add_storage($$, STORAGE_REGISTER, LOC(@2)); }
  | EXTERN struct_specifier { $$ = declspec_add_storage(NULL, STORAGE_EXTERN, LOC(@1));
                              $$ = declspec_add_type($$, $2, LOC(@2)); }
  | STATIC struct_specifier { $$ = declspec_add_storage(NULL, STORAGE_STATIC, LOC(@1));
                              $$ = declspec_add_type($$, $2, LOC(@2)); }
  | TYPEDEF struct_specifier { $$ = declspec_add_storage(NULL, STORAGE_TYPEDEF, LOC(@1));
                               $$ = declspec_add_type($$, $2, LOC(@2)); }
  | AUTO struct_specifier { $$ = declspec_add_storage(NULL, STORAGE_AUTO, LOC(@1));
                            $$ = declspec_add_type($$, $2, LOC(@2)); }
  | REGISTER struct_specifier { $$ = declspec_add_storage(NULL, STORAGE_REGISTER, LOC(@1));
                                $$ = declspec_add_type($$, $2, LOC(@2)); }
  | struct_specifier EXTERN { $$ = declspec_add_type(NULL, $1, LOC(@1));
                              $$ = declspec_add_storage($$, STORAGE_EXTERN, LOC(@2)); }
  | struct_specifier STATIC { $$ = declspec_add_type(NULL, $1, LOC(@1));
                              $$ = declspec_add_storage($$, STORAGE_STATIC, LOC(@2)); }
  | struct_specifier TYPEDEF { $$ = declspec_add_type(NULL, $1, LOC(@1));
                               $$ = declspec_add_storage($$, STORAGE_TYPEDEF, LOC(@2)); }
  | struct_specifier AUTO { $$ = declspec_add_type(NULL, $1, LOC(@1));
                            $$ = declspec_add_storage($$, STORAGE_AUTO, LOC(@2)); }
  | struct_specifier REGISTER { $$ = declspec_add_type(NULL, $1, LOC(@1));
                                $$ = declspec_add_storage($$, STORAGE_REGISTER, LOC(@2)); }
  | EXTERN union_specifier { $$ = declspec_add_storage(NULL, STORAGE_EXTERN, LOC(@1));
                             $$ = declspec_add_type($$, $2, LOC(@2)); }
  | STATIC union_specifier { $$ = declspec_add_storage(NULL, STORAGE_STATIC, LOC(@1));
                             $$ = declspec_add_type($$, $2, LOC(@2)); }
  | TYPEDEF union_specifier { $$ = declspec_add_storage(NULL, STORAGE_TYPEDEF, LOC(@1));
                              $$ = declspec_add_type($$, $2, LOC(@2)); }
  | AUTO union_specifier { $$ = declspec_add_storage(NULL, STORAGE_AUTO, LOC(@1));
                           $$ = declspec_add_type($$, $2, LOC(@2)); }
  | REGISTER union_specifier { $$ = declspec_add_storage(NULL, STORAGE_REGISTER, LOC(@1));
                               $$ = declspec_add_type($$, $2, LOC(@2)); }
  | union_specifier EXTERN { $$ = declspec_add_type(NULL, $1, LOC(@1));
                             $$ = declspec_add_storage($$, STORAGE_EXTERN, LOC(@2)); }
  | union_specifier STATIC { $$ = declspec_add_type(NULL, $1, LOC(@1));
                             $$ = declspec_add_storage($$, STORAGE_STATIC, LOC(@2)); }
  | union_specifier TYPEDEF { $$ = declspec_add_type(NULL, $1, LOC(@1));
                              $$ = declspec_add_storage($$, STORAGE_TYPEDEF, LOC(@2)); }
  | union_specifier AUTO { $$ = declspec_add_type(NULL, $1, LOC(@1));
                           $$ = declspec_add_storage($$, STORAGE_AUTO, LOC(@2)); }
  | union_specifier REGISTER { $$ = declspec_add_type(NULL, $1, LOC(@1));
                               $$ = declspec_add_storage($$, STORAGE_REGISTER, LOC(@2)); }
  | EXTERN enum_specifier { $$ = declspec_add_storage(NULL, STORAGE_EXTERN, LOC(@1));
                            $$ = declspec_add_type($$, $2, LOC(@2)); }
  | STATIC enum_specifier { $$ = declspec_add_storage(NULL, STORAGE_STATIC, LOC(@1));
                            $$ = declspec_add_type($$, $2, LOC(@2)); }
  | TYPEDEF enum_specifier { $$ = declspec_add_storage(NULL, STORAGE_TYPEDEF, LOC(@1));
                             $$ = declspec_add_type($$, $2, LOC(@2)); }
  | AUTO enum_specifier { $$ = declspec_add_storage(NULL, STORAGE_AUTO, LOC(@1));
                          $$ = declspec_add_type($$, $2, LOC(@2)); }
  | REGISTER enum_specifier { $$ = declspec_add_storage(NULL, STORAGE_REGISTER, LOC(@1));
                              $$ = declspec_add_type($$, $2, LOC(@2)); }
  | enum_specifier EXTERN { $$ = declspec_add_type(NULL, $1, LOC(@1));
                            $$ = declspec_add_storage($$, STORAGE_EXTERN, LOC(@2)); }
  | enum_specifier STATIC { $$ = declspec_add_type(NULL, $1, LOC(@1));
                            $$ = declspec_add_storage($$, STORAGE_STATIC, LOC(@2)); }
  | enum_specifier TYPEDEF { $$ = declspec_add_type(NULL, $1, LOC(@1));
                             $$ = declspec_add_storage($$, STORAGE_TYPEDEF, LOC(@2)); }
  | enum_specifier AUTO { $$ = declspec_add_type(NULL, $1, LOC(@1));
                          $$ = declspec_add_storage($$, STORAGE_AUTO, LOC(@2)); }
  | enum_specifier REGISTER { $$ = declspec_add_type(NULL, $1, LOC(@1));
                              $$ = declspec_add_storage($$, STORAGE_REGISTER, LOC(@2)); }
  | CONST named_declaration_specifiers
        { $$ = declspec_add_qualifier($2, TQ_CONST, LOC(@1)); }
  | named_declaration_specifiers CONST
        { $$ = declspec_add_qualifier($1, TQ_CONST, LOC(@2)); }
  | VOLATILE named_declaration_specifiers
        { $$ = declspec_add_qualifier($2, TQ_VOLATILE, LOC(@1)); }
  | named_declaration_specifiers VOLATILE
        { $$ = declspec_add_qualifier($1, TQ_VOLATILE, LOC(@2)); }
  ;

struct_specifier:
    STRUCT struct_tag
        { $$ = struct_tag_forward($2, LOC(@1)); }
  | STRUCT struct_tag '{' struct_declaration_list '}'
        { $$ = struct_tag_define($2, $4, LOC(@1)); }
  | STRUCT '{' struct_declaration_list '}'
        { $$ = struct_tag_define(NULL, $3, LOC(@1)); }
  ;

union_specifier:
    UNION struct_tag
        { $$ = union_tag_forward($2, LOC(@1)); }
  | UNION struct_tag '{' struct_declaration_list '}'
        { $$ = union_tag_define($2, $4, LOC(@1)); }
  | UNION '{' struct_declaration_list '}'
        { $$ = union_tag_define(NULL, $3, LOC(@1)); }
  ;

enum_specifier:
    ENUM enum_tag
        { $$ = enum_tag_forward($2, LOC(@1)); }
  | ENUM enum_tag '{' enumerator_list '}'
        { $$ = enum_tag_define($2, $4, LOC(@1)); }
  | ENUM '{' enumerator_list '}'
        { $$ = enum_tag_define(NULL, $3, LOC(@1)); }
  ;

enum_tag:
    IDENT                    { $$ = $1; }
  | TYPEDEF_NAME             { $$ = $1; }
  ;

enumerator_list:
    enumerator
  | enumerator_list ',' enumerator
        {
            Enumerator *tail = $1;
            while (tail->next)
                tail = tail->next;
            tail->next = $3;
            $$ = $1;
        }
  ;

enumerator:
    member_name
        { $$ = enumerator_new($1, NULL, LOC(@1)); }
  | member_name '=' arg_expr
        { $$ = enumerator_new($1, $3, LOC(@1)); }
  ;

/* Struct tags live in a namespace distinct from ordinary identifiers and
   typedef names (C89 3.1.2.3), so a typedef name may reappear as a tag. */
struct_tag:
    IDENT                    { $$ = $1; }
  | TYPEDEF_NAME             { $$ = $1; }
  ;

struct_declaration_list:
    struct_declaration
  | struct_declaration_list struct_declaration
        {
            StructField *tail = $1;
            while (tail->next)
                tail = tail->next;
            tail->next = $2;
            $$ = $1;
        }
  ;

struct_declaration:
    declaration_specifiers struct_declarator_list ';'
        {
            Type *ty = declspec_without_storage($1, LOC(@1), "struct member");
            StructField *f;
            for (f = $2; f; f = f->next)
                f->spec = ty;
            $$ = $2;
        }
  ;

struct_declarator_list:
    struct_decl_item
        { $$ = $1; }
  | struct_declarator_list ',' struct_decl_item
        {
            StructField *tail = $1;
            while (tail->next)
                tail = tail->next;
            tail->next = $3;
            $$ = $1;
        }
  ;

struct_decl_item:
    declarator
        { $$ = struct_field_append(NULL, NULL, $1, LOC(@1)); }
  | declarator ':' arg_expr
        { $$ = struct_field_append_bit(NULL, NULL, $1, $3, LOC(@1)); }
  | ':' arg_expr
        { $$ = struct_field_append_bit(NULL, NULL, NULL, $2, LOC(@1)); }
  ;

cast_type:
    declaration_specifiers abstract_declarator_opt
        { $$ = type_apply_declarator(
              declspec_without_storage($1, LOC(@1), "type name"), $2,
              LOC(@1)); }
  ;

declarator:
    '*' declarator           { $$ = declarator_ptr($2, TQ_NONE); }
  | '*' type_qualifier_list declarator
        { $$ = declarator_ptr($3, (unsigned)$2); }
  | direct_declarator
  ;

type_qualifier_list:
    CONST                    { $$ = TQ_CONST; }
  | VOLATILE                 { $$ = TQ_VOLATILE; }
  | type_qualifier_list CONST
        { if ($1 & TQ_CONST) diag_error_at(LOC(@2), "duplicate 'const' type qualifier");
          $$ = $1 | TQ_CONST; }
  | type_qualifier_list VOLATILE
        { if ($1 & TQ_VOLATILE) diag_error_at(LOC(@2), "duplicate 'volatile' type qualifier");
          $$ = $1 | TQ_VOLATILE; }
  ;

direct_declarator:
    IDENT
        { $$ = declarator_ident($1); }
  | TYPEDEF_NAME
        { $$ = declarator_ident($1); }
  | '(' declarator ')'
        { $$ = declarator_paren_group($2); }
  | direct_declarator '[' expr ']'
        { $$ = declarator_add_dim($1, $3, declarator_was_paren($1)); }
  | direct_declarator '[' ']'
        { $$ = declarator_add_dim($1, NULL, declarator_was_paren($1)); }
  | direct_declarator '(' param_scope_start param_clause ')'
        { (void)$3; typedef_leave_scope(); struct_tag_leave_scope();
          $$ = declarator_func($1, $4); }
  ;

abstract_declarator_opt:
    %empty                   { $$ = declarator_empty(); }
  | abstract_declarator
  ;

abstract_declarator:
    '*' abstract_declarator_opt  { $$ = declarator_ptr($2, TQ_NONE); }
  | '*' type_qualifier_list abstract_declarator_opt
        { $$ = declarator_ptr($3, (unsigned)$2); }
  | direct_abstract_declarator
  ;

direct_abstract_declarator:
    '[' expr ']'
        { $$ = declarator_add_dim(declarator_empty(), $2, 0); }
  | '[' ']'
        { $$ = declarator_add_dim(declarator_empty(), NULL, 0); }
  | '(' abstract_declarator ')'
        { $$ = declarator_paren_group($2); }
  | direct_abstract_declarator '[' expr ']'
        { $$ = declarator_add_dim($1, $3, declarator_was_paren($1)); }
  | direct_abstract_declarator '[' ']'
        { $$ = declarator_add_dim($1, NULL, declarator_was_paren($1)); }
  | direct_abstract_declarator '(' param_scope_start param_clause ')'
        { (void)$3; typedef_leave_scope(); struct_tag_leave_scope();
          $$ = declarator_func($1, $4); }
  ;

param_scope_start:
    %empty                   { typedef_enter_scope(); struct_tag_enter_scope();
                               $$ = 1; }
  ;

block_scope_start:
    %empty                   { typedef_enter_scope(); struct_tag_enter_scope();
                               $$ = 1; }
  ;

function_body_scope_start:
    %empty                   { typedef_enter_scope(); struct_tag_enter_scope();
                               $$ = 1; }
  ;

param_clause:
    %empty                   { $$ = param_clause(NULL, 0, 0); }
  | param_list
        {
            Param *h = $1;
            if (h && h->next == NULL && h->name == NULL && type_is_void(h->ty))
                $$ = param_clause(NULL, 1, 0);
            else
                $$ = param_clause(h, 1, 0);
        }
  | param_list ',' ELLIPSIS { $$ = param_clause($1, 1, 1); }
  ;

param_list:
    param                    { $$ = $1; }
  | param_list ',' param
        {
            Param *tail = $1;
            while (tail->next)
                tail = tail->next;
            tail->next = $3;
            $$ = $1;
        }
  ;

param:
    declaration_specifiers declarator
        {
            Type *ty = parameter_declspec_type($1, LOC(@1));
            $$ = param_append_decl(NULL, ty, $2, declarator_name($2));
            $$->storage = $1->storage;
            typedef_hide_name($$->name, LOC(@2));
        }
  | declaration_specifiers abstract_declarator
        { $$ = param_append_decl(NULL, parameter_declspec_type($1, LOC(@1)),
                                 $2, NULL);
          $$->storage = $1->storage; }
  | declaration_specifiers
        { $$ = param_append(NULL, parameter_declspec_type($1, LOC(@1)), NULL);
          $$->storage = $1->storage; }
  ;

stmt_list:
    %empty                   { $$ = stmt_list_new(); }
  | stmt_list stmt           { $$ = stmt_list_append($1, $2); }
  ;

stmt:
    ';'                      { $$ = node_expr_stmt(NULL, LOC(@1)); }
  | expr ';'                 { $$ = node_expr_stmt($1, LOC(@2)); }
  | RETURN expr ';'          { $$ = node_return($2, LOC(@1)); }
  | RETURN ';'               { $$ = node_return(NULL, LOC(@1)); }
  | local_declaration        { $$ = $1; }
  | IF '(' expr ')' stmt %prec IFX
                              { $$ = node_if($3, $5, NULL, LOC(@1)); }
  | IF '(' expr ')' stmt ELSE stmt
                              { $$ = node_if($3, $5, $7, LOC(@1)); }
  | WHILE '(' expr ')' stmt   { $$ = node_while($3, $5, LOC(@1)); }
  | DO stmt WHILE '(' expr ')' ';'
                              { $$ = node_do_while($2, $5, LOC(@1)); }
  | FOR '(' expr_opt ';' expr_opt ';' expr_opt ')' stmt
                              { $$ = node_for($3, $5, $7, $9, LOC(@1)); }
  | SWITCH '(' expr ')' stmt  { $$ = node_switch($3, $5, LOC(@1)); }
  | CASE conditional_expr ':' stmt
                              { $$ = node_case($2, $4, LOC(@1)); }
  | DEFAULT ':' stmt          { $$ = node_default($3, LOC(@1)); }
  | label_name ':' stmt       { $$ = node_label($1, $3, LOC(@1)); }
  | GOTO label_name ';'       { $$ = node_goto($2, LOC(@1)); }
  | BREAK ';'                { $$ = node_break(LOC(@1)); }
  | CONTINUE ';'             { $$ = node_continue(LOC(@1)); }
  | '{' block_scope_start stmt_list '}'
        { (void)$2; typedef_leave_scope(); struct_tag_leave_scope();
          $$ = node_block(stmt_list_head($3), LOC(@1)); }
  ;

local_declaration:
    declaration_specifiers
        { local_declspec = $1; }
    local_init_declarator_list ';'
        { $$ = stmt_list_head($3); local_declspec = NULL; }
  ;

local_init_declarator_list:
    local_init_declarator
        { $$ = stmt_list_append(NULL, $1); }
  | local_init_declarator_list ',' local_init_declarator
        { $$ = stmt_list_append($1, $3); }
  ;

local_init_declarator:
    declarator
        { if (local_declspec->storage != STORAGE_TYPEDEF)
              typedef_hide_name(declarator_name($1), LOC(@1)); }
    initializer_opt
        {
            Type *ty = declspec_type(local_declspec, LOC(@1));
            if (local_declspec->storage == STORAGE_TYPEDEF) {
                if ($3)
                    diag_error_at(LOC(@1), "typedef '%s' is initialized",
                                  declarator_name($1));
                typedef_declare(ty, $1, LOC(@1));
                $$ = node_typedef(ty, $1, LOC(@1));
            } else {
                $$ = node_decl(declarator_name($1), ty, $1,
                               local_declspec->storage, $3, LOC(@1));
            }
        }
  ;

label_name:
    IDENT                    { $$ = $1; }
  | TYPEDEF_NAME             { $$ = $1; }
  ;

initializer_opt:
    %empty                   { $$ = NULL; }
  | '=' initializer          { $$ = $2; }
  ;

initializer:
    arg_expr                 { $$ = $1; }
  | '{' init_list '}'        { $$ = node_init_list($2, LOC(@1)); }
  | '{' init_list ',' '}'    { $$ = node_init_list($2, LOC(@1)); }
  ;

init_list:
    %empty                     { $$ = NULL; }
  | initializer                { $$ = $1; }
  | init_list ',' initializer  { $$ = init_list_append($1, $3); }
  ;

expr_opt:
    %empty                   { $$ = NULL; }
  | expr                     { $$ = $1; }
  ;

expr:
    arg_expr                 { $$ = $1; }
  | expr ',' arg_expr        { $$ = node_binop(OP_COMMA, $1, $3, LOC(@2)); }
  ;

arg_expr:
    conditional_expr         { $$ = $1; }
  | unary_expr '=' arg_expr
                             { $$ = node_assign($1, $3, LOC(@2)); }
  | unary_expr MUL_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_MUL, $1, $3, LOC(@2)); }
  | unary_expr DIV_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_DIV, $1, $3, LOC(@2)); }
  | unary_expr MOD_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_MOD, $1, $3, LOC(@2)); }
  | unary_expr ADD_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_ADD, $1, $3, LOC(@2)); }
  | unary_expr SUB_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_SUB, $1, $3, LOC(@2)); }
  | unary_expr SHL_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_SHL, $1, $3, LOC(@2)); }
  | unary_expr SHR_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_SHR, $1, $3, LOC(@2)); }
  | unary_expr AND_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_BITAND, $1, $3, LOC(@2)); }
  | unary_expr XOR_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_BITXOR, $1, $3, LOC(@2)); }
  | unary_expr OR_ASSIGN arg_expr
                             { $$ = node_compound_assign(OP_BITOR, $1, $3, LOC(@2)); }
  ;

conditional_expr:
    logical_or_expr          { $$ = $1; }
  | logical_or_expr '?' expr ':' conditional_expr
                             { $$ = node_cond($1, $3, $5, LOC(@2)); }
  ;

logical_or_expr:
    logical_and_expr         { $$ = $1; }
  | logical_or_expr LOR logical_and_expr
                             { $$ = node_logor($1, $3, LOC(@2)); }
  ;

logical_and_expr:
    bitwise_or_expr           { $$ = $1; }
  | logical_and_expr LAND bitwise_or_expr
                             { $$ = node_logand($1, $3, LOC(@2)); }
  ;

bitwise_or_expr:
    bitwise_xor_expr         { $$ = $1; }
  | bitwise_or_expr '|' bitwise_xor_expr
                             { $$ = node_binop(OP_BITOR, $1, $3, LOC(@2)); }
  ;

bitwise_xor_expr:
    bitwise_and_expr         { $$ = $1; }
  | bitwise_xor_expr '^' bitwise_and_expr
                             { $$ = node_binop(OP_BITXOR, $1, $3, LOC(@2)); }
  ;

bitwise_and_expr:
    equality_expr            { $$ = $1; }
  | bitwise_and_expr '&' equality_expr
                             { $$ = node_binop(OP_BITAND, $1, $3, LOC(@2)); }
  ;

equality_expr:
    relational_expr         { $$ = $1; }
  | equality_expr EQ relational_expr
                             { $$ = node_binop(OP_EQ, $1, $3, LOC(@2)); }
  | equality_expr NE relational_expr
                             { $$ = node_binop(OP_NE, $1, $3, LOC(@2)); }
  ;

relational_expr:
    shift_expr              { $$ = $1; }
  | relational_expr '<' shift_expr
                             { $$ = node_binop(OP_LT, $1, $3, LOC(@2)); }
  | relational_expr LE shift_expr
                             { $$ = node_binop(OP_LE, $1, $3, LOC(@2)); }
  | relational_expr '>' shift_expr
                             { $$ = node_binop(OP_GT, $1, $3, LOC(@2)); }
  | relational_expr GE shift_expr
                             { $$ = node_binop(OP_GE, $1, $3, LOC(@2)); }
  ;

shift_expr:
    additive_expr           { $$ = $1; }
  | shift_expr SHL additive_expr
                             { $$ = node_binop(OP_SHL, $1, $3, LOC(@2)); }
  | shift_expr SHR additive_expr
                             { $$ = node_binop(OP_SHR, $1, $3, LOC(@2)); }
  ;

additive_expr:
    multiplicative_expr     { $$ = $1; }
  | additive_expr '+' multiplicative_expr
                             { $$ = node_binop(OP_ADD, $1, $3, LOC(@2)); }
  | additive_expr '-' multiplicative_expr
                             { $$ = node_binop(OP_SUB, $1, $3, LOC(@2)); }
  ;

multiplicative_expr:
    cast_expr               { $$ = $1; }
  | multiplicative_expr '*' cast_expr
                             { $$ = node_binop(OP_MUL, $1, $3, LOC(@2)); }
  | multiplicative_expr '/' cast_expr
                             { $$ = node_binop(OP_DIV, $1, $3, LOC(@2)); }
  | multiplicative_expr '%' cast_expr
                             { $$ = node_binop(OP_MOD, $1, $3, LOC(@2)); }
  ;

cast_expr:
    unary_expr              { $$ = $1; }
  | '(' cast_type ')' cast_expr
                             { $$ = node_cast($2, $4, LOC(@1)); }
  ;

unary_expr:
    postfix_expr            { $$ = $1; }
  | INC cast_expr           { $$ = node_preinc($2, LOC(@1)); }
  | DEC cast_expr           { $$ = node_predec($2, LOC(@1)); }
  | '+' cast_expr           { $$ = node_pos($2, LOC(@1)); }
  | '-' cast_expr           { $$ = node_neg($2, LOC(@1)); }
  | '~' cast_expr           { $$ = node_bitnot($2, LOC(@1)); }
  | '!' cast_expr           { $$ = node_not($2, LOC(@1)); }
  | '&' cast_expr           { $$ = node_addr($2, LOC(@1)); }
  | '*' cast_expr           { $$ = node_deref($2, LOC(@1)); }
  | SIZEOF unary_expr       { $$ = node_sizeof_expr($2, LOC(@1)); }
  | SIZEOF '(' cast_type ')' { $$ = node_sizeof_type($3, LOC(@1)); }
  ;

postfix_expr:
    primary_expr            { $$ = $1; }
  | postfix_expr '(' arg_clause ')' { $$ = node_call($1, $3, LOC(@2)); }
  | postfix_expr '[' expr ']'
        { $$ = node_deref(node_binop(OP_ADD, $1, $3, LOC(@2)), LOC(@2)); }
  | postfix_expr '.' member_name
        { $$ = node_member($1, $3, LOC(@2)); }
  | postfix_expr ARROW member_name
        { $$ = node_member(node_deref($1, LOC(@2)), $3, LOC(@2)); }
  | postfix_expr INC
        { $$ = node_postinc($1, LOC(@2)); }
  | postfix_expr DEC
        { $$ = node_postdec($1, LOC(@2)); }
  ;

primary_expr:
    NUM                      { $$ = node_num($1.val, LOC(@1));
                               $$->has_long_suffix = $1.is_long;
                               $$->has_unsigned_suffix = $1.is_unsigned;
                               $$->is_hex_literal = $1.is_hex;
                               $$->is_octal_literal = $1.is_octal;
                               $$->is_char_constant = $1.is_char;
                               $$->float_val = $1.float_val;
                               $$->is_floating_literal = $1.is_floating;
                               $$->is_float_suffix = $1.is_float_suffix;
                               $$->is_long_double_suffix = $1.is_long_double_suffix; }
  | string_literal          { $$ = $1; }
  | IDENT                    { $$ = node_var($1, LOC(@1)); }
  | BUILTIN_VA_START '(' arg_expr ',' arg_expr ')'
                             { $$ = node_builtin_va_start($3, $5, LOC(@1)); }
  | BUILTIN_VA_ARG '(' arg_expr ',' cast_type ')'
                             { $$ = node_builtin_va_arg($3, $5, LOC(@1)); }
  | BUILTIN_VA_END '(' arg_expr ')'
                             { $$ = node_builtin_va_end($3, LOC(@1)); }
  | BUILTIN_SETJMP '(' arg_expr ')'
                             { $$ = node_builtin_setjmp($3, LOC(@1)); }
  | BUILTIN_LONGJMP '(' arg_expr ',' arg_expr ')'
                             { $$ = node_builtin_longjmp($3, $5, LOC(@1)); }
  | '(' expr ')'             { $$ = $2; }
  ;

string_literal:
    STRING                   { $$ = node_string($1, LOC(@1)); }
  | string_literal STRING    { $$ = node_string_append($1, $2); }
  ;

/* Member names occupy the struct-member namespace (C89 3.1.2.3), so a name
   that is also a typedef name is still a valid member selector here. */
member_name:
    IDENT                    { $$ = $1; }
  | TYPEDEF_NAME             { $$ = $1; }
  ;

arg_clause:
    %empty                   { $$ = NULL; }
  | arg_list                 { $$ = $1; }
  ;

arg_list:
    arg_expr                 { $$ = stmt_list_append(NULL, $1); }
  | arg_list ',' arg_expr    { $$ = stmt_list_append($1, $3); }
  ;

%%

void yyerror(const char *msg)
{
    diag_error_at((SourceLoc){ yylloc.file, yylloc.first_line,
                               yylloc.first_column }, "%s", msg);
}
