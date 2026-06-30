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
    #include "type.h"
}

%union {
    long num;
    char *str;
    Node *node;
    NodeList *list;
    Param *param;
    ParamClause *pclause;
    Function *func;
    Type *type;
}

%token <num> NUM
%token <str> IDENT
%token INT CHAR VOID RETURN IF ELSE WHILE FOR
%token EQ NE LE GE

%type <node> expr expr_opt stmt
%type <list> stmt_list arg_clause arg_list
%type <param> param_list param
%type <pclause> param_clause
%type <func> toplevel
%type <type> type

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

/* Type specifier with optional pointer declarator. Left recursion on '*'
 * builds nested pointer types: `int * *` -> ptr(ptr(int)). This is a known
 * subset of the C89 declarator grammar (no arrays, no function pointers). */
type:
    INT                      { $$ = type_int(); }
  | CHAR                     { $$ = type_char(); }
  | VOID                     { $$ = type_void(); }
  | type '*'                 { $$ = type_ptr($1); }
  ;

param_clause:
    /* empty */              { $$ = param_clause(NULL, 0); }
  | param_list
        {
            /* `(void)` parses as one unnamed void param and means
             * "no parameters", not a parameter of type void. */
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
    type IDENT               { $$ = param_append(NULL, $1, $2); }
  | type                     { $$ = param_append(NULL, $1, NULL); }
  ;

stmt_list:
    /* empty */              { $$ = stmt_list_new(); }
  | stmt_list stmt           { $$ = stmt_list_append($1, $2); }
  ;

stmt:
    RETURN expr ';'          { $$ = node_return($2, LOC(@1)); }
  | RETURN ';'               { $$ = node_return(NULL, LOC(@1)); }
  | type IDENT ';'           { $$ = node_decl($2, $1, NULL, LOC(@1)); }
  | type IDENT '=' expr ';'  { $$ = node_decl($2, $1, $4, LOC(@1)); }
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
