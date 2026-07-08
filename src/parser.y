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

extern int yylex(void);
void yyerror(const char *msg);

Function *g_program = NULL;

#define LOC(L) ((SourceLoc){ (L).first_line, (L).first_column })
%}

%locations
%define parse.error verbose

%code requires {
    #include "ast.h"
    #include "token.h"
    #include "type.h"
}

%union {
    struct { long val; int is_long; int is_hex; int is_octal; } num;
    char *str;
    Node *node;
    NodeList *list;
    Param *param;
    ParamClause *pclause;
    Function *func;
    Type *type;
    Declarator *decl;
    TypedefDecl *tdecl;
    StructField *fields;
}

%token <num> NUM
%token <str> IDENT TYPEDEF_NAME
%token INT CHAR SHORT LONG VOID UNSIGNED SIGNED RETURN IF ELSE WHILE FOR SIZEOF TYPEDEF STRUCT
%token EQ NE LE GE ARROW

%type <node> expr expr_opt arg_expr stmt initializer init_list initializer_opt
%type <list> stmt_list arg_clause arg_list
%type <param> param_list param
%type <pclause> param_clause
%type <func> toplevel
%type <tdecl> typedef_toplevel
%type <type> type specifier cast_type decl_specifier keyword_specifier struct_specifier
%type <fields> struct_declaration_list struct_declaration struct_declarator_list
               struct_decl_item
%type <str> struct_tag member_name
%type <decl> declarator direct_declarator abstract_declarator struct_member_decl
                                  abstract_declarator_opt direct_abstract_declarator

%right '='
%left ','
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%precedence UMINUS
%precedence SIZEOF
%nonassoc IFX
%nonassoc ELSE
%precedence PREC_STRUCT_MEMBER_END
%right ':'

%%

program:
    /* empty */              { }
  | program toplevel         { if ($2) g_program = func_append(g_program, $2); }
  | program typedef_toplevel { g_typedef_decls = typedef_decl_append(g_typedef_decls,
                                                                     $2->spec,
                                                                     $2->decl,
                                                                     $2->loc); }
  | program struct_toplevel    { }
  ;

struct_toplevel:
    struct_specifier ';'       { (void)$1; }
  ;

typedef_toplevel:
    TYPEDEF decl_specifier declarator ';'
        {
            typedef_declare($2, $3, LOC(@1));
            $$ = typedef_decl_new($2, $3, LOC(@1));
        }
  ;

toplevel:
    type IDENT '(' param_clause ')' '{' { typedef_enter_scope(); } stmt_list '}'
        { typedef_leave_scope();
          $$ = func_new($2, $4, $1, 1, stmt_list_head($8), LOC(@2)); }
  | type IDENT '(' param_clause ')' ';'
        { $$ = func_new($2, $4, $1, 0, NULL, LOC(@2)); }
  ;

/* Integer / void specifiers (never a bare identifier). */
keyword_specifier:
    INT                      { $$ = type_int(); }
  | CHAR                     { $$ = type_char(); }
  | SHORT                    { $$ = type_short(); }
  | SHORT INT                { $$ = type_short(); }
  | LONG                     { $$ = type_long(); }
  | LONG INT                 { $$ = type_long(); }
  | VOID                     { $$ = type_void(); }
  | UNSIGNED                 { $$ = type_unsigned_int(); }
  | UNSIGNED INT             { $$ = type_unsigned_int(); }
  | UNSIGNED LONG            { $$ = type_unsigned_long(); }
  | UNSIGNED LONG INT        { $$ = type_unsigned_long(); }
  | LONG UNSIGNED            { $$ = type_unsigned_long(); }
  | LONG UNSIGNED INT        { $$ = type_unsigned_long(); }
  | UNSIGNED CHAR            { $$ = type_unsigned_char(); }
  | UNSIGNED SHORT           { $$ = type_unsigned_short(); }
  | UNSIGNED SHORT INT       { $$ = type_unsigned_short(); }
  | SHORT UNSIGNED           { $$ = type_unsigned_short(); }
  | SHORT UNSIGNED INT       { $$ = type_unsigned_short(); }
  | SIGNED                   { $$ = type_int(); }
  | SIGNED INT               { $$ = type_int(); }
  | SIGNED CHAR              { $$ = type_signed_char(); }
  | SIGNED SHORT             { $$ = type_short(); }
  | SIGNED SHORT INT         { $$ = type_short(); }
  | SIGNED LONG              { $$ = type_long(); }
  | SIGNED LONG INT          { $$ = type_long(); }
  ;

/* Declaration specifier: keywords, typedef name, or struct specifier. */
decl_specifier:
    keyword_specifier        { $$ = $1; }
  | TYPEDEF_NAME             { $$ = typedef_lookup($1); }
  | struct_specifier         { $$ = $1; }
  ;

struct_specifier:
    STRUCT struct_tag
        { $$ = struct_tag_forward($2, LOC(@1)); }
  | STRUCT struct_tag '{' struct_declaration_list '}'
        { $$ = struct_tag_define($2, $4, LOC(@1)); }
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
    decl_specifier struct_declarator_list ';'
        {
            StructField *f;
            for (f = $2; f; f = f->next)
                f->spec = $1;
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
    struct_member_decl
        { $$ = struct_field_append(NULL, NULL, $1, LOC(@1)); }
  | ':' expr
        { $$ = struct_field_append_bit(NULL, NULL, NULL, $2, LOC(@1)); }
  ;

struct_member_decl:
    IDENT ':' expr
        { $$ = declarator_bitfield(declarator_ident($1), $3); }
  | IDENT %prec PREC_STRUCT_MEMBER_END
        { $$ = declarator_ident($1); }
  | '*' struct_member_decl
        { $$ = declarator_ptr($2); }
  | '(' struct_member_decl ')'
        { $$ = declarator_paren_group($2); }
  | struct_member_decl '[' expr ']'
        { $$ = declarator_add_dim($1, $3, declarator_was_paren($1)); }
  | struct_member_decl '[' ']'
        { $$ = declarator_add_dim($1, NULL, declarator_was_paren($1)); }
  ;

specifier:
    decl_specifier             { $$ = $1; }
  ;

/* Function return types may still use a trailing `*` prefix. */
type:
    specifier                { $$ = $1; }
  | type '*'                 { $$ = type_ptr($1); }
  ;

cast_type:
    decl_specifier abstract_declarator_opt
        { $$ = type_apply_declarator($1, $2, LOC(@1)); }
  ;

declarator:
    '*' declarator           { $$ = declarator_ptr($2); }
  | direct_declarator
  ;

direct_declarator:
    IDENT %prec PREC_STRUCT_MEMBER_END
        { $$ = declarator_ident($1); }
  | TYPEDEF_NAME
        { $$ = declarator_ident($1); }
  | '(' declarator ')'
        { $$ = declarator_paren_group($2); }
  | direct_declarator '[' expr ']'
        { $$ = declarator_add_dim($1, $3, declarator_was_paren($1)); }
  | direct_declarator '[' ']'
        { $$ = declarator_add_dim($1, NULL, declarator_was_paren($1)); }
  ;

abstract_declarator_opt:
    /* empty */              { $$ = declarator_empty(); }
  | abstract_declarator
  ;

abstract_declarator:
    '*' abstract_declarator_opt  { $$ = declarator_ptr($2); }
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
  ;

param_clause:
    /* empty */              { $$ = param_clause(NULL, 0); }
  | param_list
        {
            Param *h = $1;
            if (h && h->next == NULL && h->name == NULL && type_is_void(h->ty))
                $$ = param_clause(NULL, 1);
            else
                $$ = param_clause(h, 1);
        }
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
    decl_specifier declarator
        {
            $$ = param_append_decl(NULL, $1, $2, declarator_name($2));
        }
  | decl_specifier
        { $$ = param_append(NULL, $1, NULL); }
  ;

stmt_list:
    /* empty */              { $$ = stmt_list_new(); }
  | stmt_list stmt           { $$ = stmt_list_append($1, $2); }
  ;

stmt:
    expr ';'                 { $$ = node_expr_stmt($1, LOC(@2)); }
  | RETURN expr ';'          { $$ = node_return($2, LOC(@1)); }
  | RETURN ';'               { $$ = node_return(NULL, LOC(@1)); }
  | TYPEDEF decl_specifier declarator ';'
        {
            typedef_declare($2, $3, LOC(@1));
            $$ = node_typedef($2, $3, LOC(@1));
        }
  | decl_specifier declarator initializer_opt ';'
        {
            $$ = node_decl(declarator_name($2), $1, $2, $3, LOC(@1));
        }
  | IF '(' expr ')' stmt %prec IFX
                              { $$ = node_if($3, $5, NULL, LOC(@1)); }
  | IF '(' expr ')' stmt ELSE stmt
                              { $$ = node_if($3, $5, $7, LOC(@1)); }
  | WHILE '(' expr ')' stmt   { $$ = node_while($3, $5, LOC(@1)); }
  | FOR '(' expr_opt ';' expr_opt ';' expr_opt ')' stmt
                              { $$ = node_for($3, $5, $7, $9, LOC(@1)); }
  | '{' { typedef_enter_scope(); } stmt_list '}'
        { typedef_leave_scope();
          $$ = node_block(stmt_list_head($3), LOC(@1)); }
  ;

initializer_opt:
    /* empty */              { $$ = NULL; }
  | '=' initializer          { $$ = $2; }
  ;

initializer:
    arg_expr                 { $$ = $1; }
  | '{' init_list '}'        { $$ = node_init_list($2, LOC(@1)); }
  | '{' init_list ',' '}'    { $$ = node_init_list($2, LOC(@1)); }
  ;

init_list:
    /* empty */                { $$ = NULL; }
  | initializer                { $$ = $1; }
  | init_list ',' initializer  { $$ = init_list_append($1, $3); }
  ;

expr_opt:
    /* empty */              { $$ = NULL; }
  | expr                     { $$ = $1; }
  ;

expr:
    arg_expr                 { $$ = $1; }
  | expr ',' arg_expr        { $$ = node_binop(OP_COMMA, $1, $3, LOC(@2)); }
  ;

arg_expr:
    NUM                      { $$ = node_num($1.val, LOC(@1));
                               $$->has_long_suffix = $1.is_long;
                               $$->is_hex_literal = $1.is_hex;
                               $$->is_octal_literal = $1.is_octal; }
  | IDENT                    { $$ = node_var($1, LOC(@1)); }
  | IDENT '(' arg_clause ')' { $$ = node_call($1, $3, LOC(@1)); }
  | arg_expr '=' arg_expr    { $$ = node_assign($1, $3, LOC(@2)); }
  | arg_expr '+' arg_expr    { $$ = node_binop(OP_ADD, $1, $3, LOC(@2)); }
  | arg_expr '-' arg_expr    { $$ = node_binop(OP_SUB, $1, $3, LOC(@2)); }
  | arg_expr '*' arg_expr    { $$ = node_binop(OP_MUL, $1, $3, LOC(@2)); }
  | arg_expr '/' arg_expr    { $$ = node_binop(OP_DIV, $1, $3, LOC(@2)); }
  | arg_expr '%' arg_expr    { $$ = node_binop(OP_MOD, $1, $3, LOC(@2)); }
  | arg_expr EQ arg_expr     { $$ = node_binop(OP_EQ, $1, $3, LOC(@2)); }
  | arg_expr NE arg_expr     { $$ = node_binop(OP_NE, $1, $3, LOC(@2)); }
  | arg_expr '<' arg_expr    { $$ = node_binop(OP_LT, $1, $3, LOC(@2)); }
  | arg_expr LE arg_expr     { $$ = node_binop(OP_LE, $1, $3, LOC(@2)); }
  | arg_expr '>' arg_expr    { $$ = node_binop(OP_GT, $1, $3, LOC(@2)); }
  | arg_expr GE arg_expr     { $$ = node_binop(OP_GE, $1, $3, LOC(@2)); }
  | '-' arg_expr %prec UMINUS { $$ = node_neg($2, LOC(@1)); }
  | '&' arg_expr %prec UMINUS { $$ = node_addr($2, LOC(@1)); }
  | '*' arg_expr %prec UMINUS { $$ = node_deref($2, LOC(@1)); }
  | SIZEOF arg_expr %prec SIZEOF { $$ = node_sizeof_expr($2, LOC(@1)); }
  | SIZEOF '(' cast_type ')' %prec SIZEOF { $$ = node_sizeof_type($3, LOC(@1)); }
  | '(' cast_type ')' arg_expr %prec UMINUS
                             { $$ = node_cast($2, $4, LOC(@1)); }
  | arg_expr '[' arg_expr ']'
        { $$ = node_deref(node_binop(OP_ADD, $1, $3, LOC(@2)), LOC(@2)); }
  | arg_expr '.' member_name
        { $$ = node_member($1, $3, LOC(@2)); }
  | arg_expr ARROW member_name
        { $$ = node_member(node_deref($1, LOC(@2)), $3, LOC(@2)); }
  | '(' expr ')'             { $$ = $2; }
  ;

/* Member names occupy the struct-member namespace (C89 3.1.2.3), so a name
   that is also a typedef name is still a valid member selector here. */
member_name:
    IDENT                    { $$ = $1; }
  | TYPEDEF_NAME             { $$ = $1; }
  ;

arg_clause:
    /* empty */              { $$ = NULL; }
  | arg_list                 { $$ = $1; }
  ;

arg_list:
    arg_expr                 { $$ = stmt_list_append(NULL, $1); }
  | arg_list ',' arg_expr    { $$ = stmt_list_append($1, $3); }
  ;

%%

void yyerror(const char *msg)
{
    diag_error_at((SourceLoc){ yylloc.first_line, yylloc.first_column },
                  "%s", msg);
}
