/* SPDX-License-Identifier: MIT */
%option noyywrap yylineno

%{
#include <stdlib.h>
#include <errno.h>

#include "token.h"
#include "ast.h"
#include "arena.h"
#include "diag.h"
#include "parser.h"

int yycolumn = 1;

/* Decode a decimal integer constant, flagging C89 3.1.5 range overflow.
 * strtol stops at any trailing L/l suffix, so the same routine serves both. */
static long scan_decimal(const char *text)
{
    long v;

    errno = 0;
    v = strtol(text, NULL, 10);
    if (errno == ERANGE)
        diag_error_at((SourceLoc){ yylineno, yycolumn },
                      "integer constant out of range");
    return v;
}

static long scan_hex(const char *text)
{
    long v;

    errno = 0;
    v = strtol(text, NULL, 16);
    if (errno == ERANGE)
        diag_error_at((SourceLoc){ yylineno, yycolumn },
                      "integer constant out of range");
    return v;
}

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
"short"             { return SHORT; }
"long"              { return LONG; }
"void"              { return VOID; }
"unsigned"          { return UNSIGNED; }
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
0[xX][0-9a-fA-F]+       { yylval.num.val = scan_hex(yytext);
                          yylval.num.is_long = 0;
                          yylval.num.is_hex = 1; return NUM; }
[0-9]+[Ll]              { yylval.num.val = scan_decimal(yytext);
                          yylval.num.is_long = 1;
                          yylval.num.is_hex = 0; return NUM; }
[0-9]+                  { yylval.num.val = scan_decimal(yytext);
                          yylval.num.is_long = 0;
                          yylval.num.is_hex = 0; return NUM; }
[0-9]+[A-Za-z_]+        { diag_error_at((SourceLoc){ yylineno, yycolumn },
                                        "invalid suffix on integer constant");
                          yylval.num.val = scan_decimal(yytext);
                          yylval.num.is_long = 0;
                          yylval.num.is_hex = 0; return NUM; }

[-+*/%=<>(){};,&\[\]]   { return yytext[0]; }

.                   { diag_error_at((SourceLoc){ yylineno, yycolumn },
                                    "stray '%c' in program", yytext[0]); }

%%
