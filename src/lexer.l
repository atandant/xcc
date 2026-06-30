/* SPDX-License-Identifier: MIT */
%option noyywrap yylineno

%{
#include <stdlib.h>

#include "token.h"
#include "ast.h"
#include "arena.h"
#include "diag.h"
#include "parser.h"

int yycolumn = 1;

/* Maintain yylloc (column tracking) for every token; bison's %locations
 * gives us yylineno-based line tracking automatically. */
#define YY_USER_ACTION                                  \
    yylloc.first_line = yylloc.last_line = yylineno;    \
    yylloc.first_column = yycolumn;                      \
    yylloc.last_column = yycolumn + yyleng - 1;          \
    yycolumn += yyleng;
%}

%%

[ \t\r]+            { /* whitespace */ }
\n                  { yycolumn = 1; }
"//".*              { /* comment */ }
"/*"([^*]|\*+[^*/])*\*+"/" { /* comment */ }

"int"               { return INT; }
"char"              { return CHAR; }
"void"              { return VOID; }
"return"            { return RETURN; }
"if"                { return IF; }
"else"              { return ELSE; }
"while"             { return WHILE; }
"for"               { return FOR; }

"=="                { return EQ; }
"!="                { return NE; }
"<="                { return LE; }
">="                { return GE; }

[A-Za-z_][A-Za-z0-9_]*  { yylval.str = arena_strdup(yytext); return IDENT; }
[0-9]+                  { yylval.num = strtol(yytext, NULL, 10); return NUM; }

[-+*/%=<>(){};,&]   { return yytext[0]; }

.                   { diag_error_at((SourceLoc){ yylineno, yycolumn },
                                    "stray '%c' in program", yytext[0]); }

%%
