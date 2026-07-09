/* SPDX-License-Identifier: MIT */
%option noyywrap yylineno

%{
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "token.h"
#include "ast.h"
#include "arena.h"
#include "diag.h"
#include "parser.h"
#include "sema_typedef.h"

int yycolumn = 1;

static SourceLoc scan_loc(void)
{
    return (SourceLoc){ yylineno, yycolumn };
}

/* Decode a decimal integer constant, flagging C89 3.1.5 range overflow.
 * strtol stops at any trailing L/l suffix, so the same routine serves both. */
static long scan_decimal(const char *text)
{
    long v;

    errno = 0;
    v = strtol(text, NULL, 10);
    if (errno == ERANGE)
        diag_error_at(scan_loc(), "integer constant out of range");
    return v;
}

static long scan_hex(const char *text)
{
    unsigned long v;

    errno = 0;
    v = strtoul(text, NULL, 16);
    if (errno == ERANGE)
        diag_error_at(scan_loc(), "integer constant out of range");
    return (long)v;
}

static long scan_octal(const char *text)
{
    const char *p;
    unsigned long v;

    for (p = text + 1; *p; p++) {
        if (*p < '0' || *p > '7') {
            diag_error_at(scan_loc(), "invalid digit in octal constant");
            break;
        }
    }

    errno = 0;
    v = strtoul(text, NULL, 8);
    if (errno == ERANGE)
        diag_error_at(scan_loc(), "integer constant out of range");
    return (long)v;
}

static int has_long_suffix(const char *text)
{
    size_t n = strlen(text);

    if (n == 0)
        return 0;
    return text[n - 1] == 'L' || text[n - 1] == 'l';
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
"signed"            { return SIGNED; }
"return"            { return RETURN; }
"if"                { return IF; }
"else"              { return ELSE; }
"while"             { return WHILE; }
"for"               { return FOR; }
"typedef"           { return TYPEDEF; }
"struct"            { return STRUCT; }
"union"             { return UNION; }
"enum"              { return ENUM; }
"sizeof"            { return SIZEOF; }

"->"                { return ARROW; }
"=="                { return EQ; }
"!="                { return NE; }
"<="                { return LE; }
">="                { return GE; }

[A-Za-z_][A-Za-z0-9_]*  {
                              yylval.str = arena_strdup(yytext);
                              if (typedef_lookup(yytext))
                                  return TYPEDEF_NAME;
                              return IDENT;
                          }
0[xX][0-9a-fA-F]+([Ll])? { yylval.num.val = scan_hex(yytext);
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_hex = 1;
                          yylval.num.is_octal = 0; return NUM; }
0[89][0-9]*([Ll])?  { diag_error_at(scan_loc(), "invalid digit in octal constant");
                          yylval.num.val = 0;
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 1; return NUM; }
0[0-7]+([Ll])?      { yylval.num.val = scan_octal(yytext);
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 1; return NUM; }
0([Ll])?            { yylval.num.val = 0;
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 1; return NUM; }
[1-9][0-9]*([Ll])?  { yylval.num.val = scan_decimal(yytext);
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 0; return NUM; }
[0-9]+[A-Za-z_]+    { diag_error_at(scan_loc(),
                                    "invalid suffix on integer constant");
                          yylval.num.val = scan_decimal(yytext);
                          yylval.num.is_long = 0;
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 0; return NUM; }

[-+*/%=<>(){};,&.:\[\]]  { return yytext[0]; }

.                   { diag_error_at(scan_loc(),
                                    "stray '%c' in program", yytext[0]); }

%%
