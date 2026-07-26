/* SPDX-License-Identifier: MIT */
%option noyywrap

%x COMMENT

%{
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "token.h"
#include "ast.h"
#include "arena.h"
#include "diag.h"
#include "parser.h"
#include "sema_typedef.h"

int yycolumn = 1;
static int yyline = 1;
static SourceLoc comment_loc;

static SourceLoc scan_loc(void)
{
    return (SourceLoc){ yylloc.first_line, yylloc.first_column };
}

static void scan_advance_location(const char *text, int len)
{
    int i;

    yylloc.first_line = yyline;
    yylloc.first_column = yycolumn;
    yylloc.last_line = yyline;
    yylloc.last_column = yycolumn;

    for (i = 0; i < len; i++) {
        yylloc.last_line = yyline;
        yylloc.last_column = yycolumn;
        if (text[i] == '\n') {
            yyline++;
            yycolumn = 1;
        } else {
            yycolumn++;
        }
    }
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

static int hex_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

static StringToken *scan_string(const char *text, int len, int quote)
{
    StringToken *token = arena_alloc(sizeof(*token));
    unsigned char *data = arena_alloc((size_t)(len > 0 ? len : 1));
    int out = 0;

    for (int i = quote + 1; i < len - 1; i++) {
        unsigned long value;
        int digits;
        int c = (unsigned char)text[i];

        if (c != '\\') {
            data[out++] = (unsigned char)c;
            continue;
        }
        i++;
        c = (unsigned char)text[i];
        if (c == '\n')
            continue;
        if (c == '\r' && i + 1 < len - 1 && text[i + 1] == '\n') {
            i++;
            continue;
        }
        switch (c) {
        case '\'': data[out++] = '\''; continue;
        case '"':  data[out++] = '"';  continue;
        case '?':  data[out++] = '?';  continue;
        case '\\': data[out++] = '\\'; continue;
        case 'a':  data[out++] = '\a'; continue;
        case 'b':  data[out++] = '\b'; continue;
        case 'f':  data[out++] = '\f'; continue;
        case 'n':  data[out++] = '\n'; continue;
        case 'r':  data[out++] = '\r'; continue;
        case 't':  data[out++] = '\t'; continue;
        case 'v':  data[out++] = '\v'; continue;
        case 'x':
            if (i + 1 >= len - 1 || !((text[i + 1] >= '0' && text[i + 1] <= '9') ||
                                      (text[i + 1] >= 'a' && text[i + 1] <= 'f') ||
                                      (text[i + 1] >= 'A' && text[i + 1] <= 'F'))) {
                diag_error_at(scan_loc(), "hex escape sequence has no digits");
                data[out++] = 0;
                continue;
            }
            value = 0;
            while (i + 1 < len - 1 &&
                   ((text[i + 1] >= '0' && text[i + 1] <= '9') ||
                    (text[i + 1] >= 'a' && text[i + 1] <= 'f') ||
                    (text[i + 1] >= 'A' && text[i + 1] <= 'F'))) {
                int digit = hex_value((unsigned char)text[++i]);
                if (value <= (ULONG_MAX - (unsigned long)digit) / 16)
                    value = value * 16 + (unsigned long)digit;
                else
                    value = ULONG_MAX;
            }
            if (value > UCHAR_MAX)
                diag_error_at(scan_loc(), "hex escape sequence out of range");
            data[out++] = (unsigned char)value;
            continue;
        default:
            if (c >= '0' && c <= '7') {
                value = 0;
                digits = 0;
                do {
                    value = value * 8 + (unsigned long)(text[i] - '0');
                    i++;
                    digits++;
                } while (digits < 3 && i < len - 1 &&
                         text[i] >= '0' && text[i] <= '7');
                i--;
                if (value > UCHAR_MAX)
                    diag_error_at(scan_loc(), "octal escape sequence out of range");
                data[out++] = (unsigned char)value;
                continue;
            }
            diag_error_at(scan_loc(), "unknown escape sequence '\\%c'", c);
            data[out++] = (unsigned char)c;
            continue;
        }
    }
    token->data = data;
    token->len = out;
    return token;
}

/* Track every matched byte so multiline comments and future multiline tokens
 * leave the next token with an exact line and column. */
#define YY_USER_ACTION scan_advance_location(yytext, yyleng);
%}

%%

[ \t\r]+            { /* whitespace */ }
\n                  { /* location advanced by YY_USER_ACTION */ }
"//".*              { /* comment */ }
"/*"                { comment_loc = scan_loc(); BEGIN(COMMENT); }
<COMMENT>[^*\n]+     { /* comment body */ }
<COMMENT>\n          { /* comment body */ }
<COMMENT>"*"+"/"   { BEGIN(INITIAL); }
<COMMENT>"*"+       { /* comment body */ }
<COMMENT><<EOF>>    {
                          diag_error_at(comment_loc, "unterminated comment");
                          yyterminate();
                      }

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
"do"                { return DO; }
"for"               { return FOR; }
"switch"            { return SWITCH; }
"case"              { return CASE; }
"default"           { return DEFAULT; }
"goto"              { return GOTO; }
"break"             { return BREAK; }
"continue"          { return CONTINUE; }
"typedef"           { return TYPEDEF; }
"struct"            { return STRUCT; }
"union"             { return UNION; }
"enum"              { return ENUM; }
"sizeof"            { return SIZEOF; }

"++"                { return INC; }
"--"                { return DEC; }
"->"                { return ARROW; }
"&&"                { return LAND; }
"||"                { return LOR; }
"<<"                { return SHL; }
">>"                { return SHR; }
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

L\"([^\\\n\"]|\\(.|\r?\n))*\" {
                          diag_error_at(scan_loc(),
                                        "wide string literals are not yet supported");
                          yylval.string = scan_string(yytext, yyleng, 1);
                          return STRING;
                      }
\"([^\\\n\"]|\\(.|\r?\n))*\" {
                          yylval.string = scan_string(yytext, yyleng, 0);
                          return STRING;
                      }
L?\"([^\\\n\"]|\\(.|\r?\n))* {
                          diag_error_at(scan_loc(),
                                        "unterminated character string literal");
                      }

[-+*/%=<>(){};,&|^.:?!\[\]]  { return yytext[0]; }

.                   { diag_error_at(scan_loc(),
                                    "stray '%c' in program", yytext[0]); }

%%
