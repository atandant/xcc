/* SPDX-License-Identifier: MIT */
%{
#include <stdio.h>
#include <stdlib.h>

#include "token.h"
#include "ast.h"
#include "diag.h"

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
    struct { long val; int is_long; int is_hex; } num;
    char *str;
    Node *node;
    NodeList *list;
    Param *param;
    ParamClause *pclause;
    Function *func;
    Type *type;
    Declarator *decl;
}

%token <num> NUM
%token <str> IDENT
%token INT CHAR SHORT LONG VOID UNSIGNED RETURN IF ELSE WHILE FOR
%token EQ NE LE GE

%type <node> expr expr_opt stmt
%type <list> stmt_list arg_clause arg_list
%type <param> param_list param
%type <pclause> param_clause
%type <func> toplevel
%type <type> type specifier cast_type
%type <decl> declarator direct_declarator abstract_declarator
                                  abstract_declarator_opt direct_abstract_declarator

%right '='
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%precedence UMINUS
%nonassoc IFX
%nonassoc ELSE

%%

program:
    /* empty */              { }
  | program toplevel         { g_program = func_append(g_program, $2); }
  ;

toplevel:
    type IDENT '(' param_clause ')' '{' stmt_list '}'
        { $$ = func_new($2, $4, $1, 1, stmt_list_head($7), LOC(@2)); }
  | type IDENT '(' param_clause ')' ';'
        { $$ = func_new($2, $4, $1, 0, NULL, LOC(@2)); }
  ;

/* Base type specifier (no declarator). */
specifier:
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
  ;

/* Function return types may still use a trailing `*` prefix. */
type:
    specifier                { $$ = $1; }
  | type '*'                 { $$ = type_ptr($1); }
  ;

cast_type:
    specifier abstract_declarator_opt
        { $$ = type_apply_declarator($1, $2, LOC(@1)); }
  ;

declarator:
    '*' declarator           { $$ = declarator_ptr($2); }
  | direct_declarator
  ;

direct_declarator:
    IDENT
        { $$ = declarator_ident($1); }
  | '(' declarator ')'
        { $$ = declarator_paren_group($2); }
  | direct_declarator '[' NUM ']'
        { $$ = declarator_add_dim($1, $3.val, declarator_was_paren($1)); }
  | direct_declarator '[' ']'
        { $$ = declarator_add_dim($1, 0, declarator_was_paren($1)); }
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
    '(' abstract_declarator ')'
        { $$ = declarator_paren_group($2); }
  | direct_abstract_declarator '[' NUM ']'
        { $$ = declarator_add_dim($1, $3.val, declarator_was_paren($1)); }
  | direct_abstract_declarator '[' ']'
        { $$ = declarator_add_dim($1, 0, declarator_was_paren($1)); }
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
  | param_list ',' param     { $$ = param_append($1, $3->ty, $3->name); }
  ;

param:
    specifier declarator
        {
            Type *ty = type_apply_declarator($1, $2, LOC(@1));
            $$ = param_append(NULL, ty, declarator_name($2));
        }
  | specifier
        { $$ = param_append(NULL, $1, NULL); }
  ;

stmt_list:
    /* empty */              { $$ = stmt_list_new(); }
  | stmt_list stmt           { $$ = stmt_list_append($1, $2); }
  ;

stmt:
    RETURN expr ';'          { $$ = node_return($2, LOC(@1)); }
  | RETURN ';'               { $$ = node_return(NULL, LOC(@1)); }
  | specifier declarator ';'
        {
            Type *ty = type_apply_declarator($1, $2, LOC(@1));
            $$ = node_decl(declarator_name($2), ty, NULL, LOC(@1));
        }
  | specifier declarator '=' expr ';'
        {
            Type *ty = type_apply_declarator($1, $2, LOC(@1));
            $$ = node_decl(declarator_name($2), ty, $4, LOC(@1));
        }
  | IF '(' expr ')' stmt %prec IFX
                              { $$ = node_if($3, $5, NULL, LOC(@1)); }
  | IF '(' expr ')' stmt ELSE stmt
                              { $$ = node_if($3, $5, $7, LOC(@1)); }
  | WHILE '(' expr ')' stmt   { $$ = node_while($3, $5, LOC(@1)); }
  | FOR '(' expr_opt ';' expr_opt ';' expr_opt ')' stmt
                              { $$ = node_for($3, $5, $7, $9, LOC(@1)); }
  | '{' stmt_list '}'         { $$ = node_block(stmt_list_head($2), LOC(@1)); }
  | expr ';'                 { $$ = node_expr_stmt($1, LOC(@1)); }
  ;

expr_opt:
    /* empty */              { $$ = NULL; }
  | expr                     { $$ = $1; }
  ;

expr:
    NUM                      { $$ = node_num($1.val, LOC(@1));
                               $$->has_long_suffix = $1.is_long;
                               $$->is_hex_literal = $1.is_hex; }
  | IDENT                    { $$ = node_var($1, LOC(@1)); }
  | IDENT '(' arg_clause ')' { $$ = node_call($1, $3, LOC(@1)); }
  | expr '=' expr            { $$ = node_assign($1, $3, LOC(@2)); }
  | expr '+' expr            { $$ = node_binop(OP_ADD, $1, $3, LOC(@2)); }
  | expr '-' expr            { $$ = node_binop(OP_SUB, $1, $3, LOC(@2)); }
  | expr '*' expr            { $$ = node_binop(OP_MUL, $1, $3, LOC(@2)); }
  | expr '/' expr            { $$ = node_binop(OP_DIV, $1, $3, LOC(@2)); }
  | expr '%' expr            { $$ = node_binop(OP_MOD, $1, $3, LOC(@2)); }
  | expr EQ expr             { $$ = node_binop(OP_EQ, $1, $3, LOC(@2)); }
  | expr NE expr             { $$ = node_binop(OP_NE, $1, $3, LOC(@2)); }
  | expr '<' expr            { $$ = node_binop(OP_LT, $1, $3, LOC(@2)); }
  | expr LE expr             { $$ = node_binop(OP_LE, $1, $3, LOC(@2)); }
  | expr '>' expr            { $$ = node_binop(OP_GT, $1, $3, LOC(@2)); }
  | expr GE expr             { $$ = node_binop(OP_GE, $1, $3, LOC(@2)); }
  | '-' expr %prec UMINUS    { $$ = node_neg($2, LOC(@1)); }
  | '&' expr %prec UMINUS    { $$ = node_addr($2, LOC(@1)); }
  | '*' expr %prec UMINUS    { $$ = node_deref($2, LOC(@1)); }
  | '(' cast_type ')' expr %prec UMINUS
                             { $$ = node_cast($2, $4, LOC(@1)); }
  | expr '[' expr ']'
        { $$ = node_deref(node_binop(OP_ADD, $1, $3, LOC(@2)), LOC(@2)); }
  | '(' expr ')'             { $$ = $2; }
  ;

arg_clause:
    /* empty */              { $$ = NULL; }
  | arg_list                 { $$ = $1; }
  ;

arg_list:
    expr                     { $$ = stmt_list_append(NULL, $1); }
  | arg_list ',' expr        { $$ = stmt_list_append($1, $3); }
  ;

%%

void yyerror(const char *msg)
{
    diag_error_at((SourceLoc){ yylloc.first_line, yylloc.first_column },
                  "%s", msg);
}
