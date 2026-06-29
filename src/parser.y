/* SPDX-License-Identifier: MIT */
%{
#include <stdio.h>
#include <stdlib.h>

#include "token.h"
#include "ast.h"
#include "diag.h"

extern int yylex(void);
void yyerror(const char *msg);

/* Result of a successful parse. */
Function *g_program = NULL;

/* Convert a bison location to our SourceLoc. */
#define LOC(L) ((SourceLoc){ (L).first_line, (L).first_column })
%}

%locations
%define parse.error verbose

%code requires {
    #include "ast.h"
    #include "token.h"
}

%union {
    long num;
    char *str;
    Node *node;
    NodeList *list;
}

%token <num> NUM
%token <str> IDENT
%token INT VOID RETURN IF ELSE WHILE FOR
%token EQ NE LE GE

%type <node> expr expr_opt stmt
%type <list> stmt_list

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
    INT IDENT '(' VOID ')' '{' stmt_list '}'
        { g_program = func_new($2, stmt_list_head($7)); }
  ;

stmt_list:
    /* empty */              { $$ = stmt_list_new(); }
  | stmt_list stmt           { $$ = stmt_list_append($1, $2); }
  ;

stmt:
    RETURN expr ';'          { $$ = node_return($2, LOC(@1)); }
  | INT IDENT ';'            { $$ = node_decl($2, NULL, LOC(@1)); }
  | INT IDENT '=' expr ';'   { $$ = node_decl($2, $4, LOC(@1)); }
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
    NUM                      { $$ = node_num($1, LOC(@1)); }
  | IDENT                    { $$ = node_var($1, LOC(@1)); }
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
  | '(' expr ')'             { $$ = $2; }
  ;

%%

void yyerror(const char *msg)
{
    diag_error_at((SourceLoc){ yylloc.first_line, yylloc.first_column },
                  "%s", msg);
}
