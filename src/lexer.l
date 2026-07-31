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
 * strtol/strtoul stop at trailing U/u/L/l suffixes. */
static long scan_decimal(const char *text, int is_unsigned)
{
    unsigned long uv;
    long v;

    errno = 0;
    if (is_unsigned) {
        uv = strtoul(text, NULL, 10);
        if (errno == ERANGE)
            diag_error_at(scan_loc(), "integer constant out of range");
        return (long)uv;
    }
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
        if (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L')
            break;
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
    return strchr(text, 'L') != NULL || strchr(text, 'l') != NULL;
}

static int has_unsigned_suffix(const char *text)
{
    return strchr(text, 'U') != NULL || strchr(text, 'u') != NULL;
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

/* Multi-character constants use the conventional left-to-right byte packing:
 * 'ab' is 0x6162.  XCC's target int is 32 bits, so at most four decoded bytes
 * are representable. */
static long scan_character(const char *text, int len)
{
    StringToken *token = scan_string(text, len, 0);
    unsigned long value = 0;
    int i;

    if (token->len == 0) {
        diag_error_at(scan_loc(), "empty character constant");
        return 0;
    }
    if (token->len > 4) {
        diag_error_at(scan_loc(), "character constant exceeds 4 bytes");
        return 0;
    }
    for (i = 0; i < token->len; i++)
        value = (value << 8) | token->data[i];
    return (long)value;
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
"extern"            { return EXTERN; }
"static"            { return STATIC; }
"auto"              { return AUTO; }
"register"          { return REGISTER; }
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
0[xX][0-9a-fA-F]+([uU]|[lL]|[uU][lL]|[lL][uU])? {
                          yylval.num.val = scan_hex(yytext);
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_unsigned = has_unsigned_suffix(yytext);
                          yylval.num.is_hex = 1;
                          yylval.num.is_octal = 0;
                          yylval.num.is_char = 0; return NUM; }
0[89][0-9]*([uU]|[lL]|[uU][lL]|[lL][uU])? {
                          diag_error_at(scan_loc(), "invalid digit in octal constant");
                          yylval.num.val = 0;
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_unsigned = has_unsigned_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 1;
                          yylval.num.is_char = 0; return NUM; }
0[0-7]+([uU]|[lL]|[uU][lL]|[lL][uU])? {
                          yylval.num.val = scan_octal(yytext);
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_unsigned = has_unsigned_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 1;
                          yylval.num.is_char = 0; return NUM; }
0([uU]|[lL]|[uU][lL]|[lL][uU])? {
                          yylval.num.val = 0;
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_unsigned = has_unsigned_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 1;
                          yylval.num.is_char = 0; return NUM; }
[1-9][0-9]*([uU]|[lL]|[uU][lL]|[lL][uU])? {
                          yylval.num.is_unsigned = has_unsigned_suffix(yytext);
                          yylval.num.val = scan_decimal(yytext, yylval.num.is_unsigned);
                          yylval.num.is_long = has_long_suffix(yytext);
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 0;
                          yylval.num.is_char = 0; return NUM; }
[0-9]+[A-Za-z_]+    { diag_error_at(scan_loc(),
                                    "invalid suffix on integer constant");
                          yylval.num.val = scan_decimal(yytext, 0);
                          yylval.num.is_long = 0;
                          yylval.num.is_unsigned = 0;
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 0;
                          yylval.num.is_char = 0; return NUM; }

L\'([^\\\n\']|\\(.|\r?\n))*\' {
                          diag_error_at(scan_loc(),
                                        "wide character constants are not yet supported");
                          yylval.num.val = 0;
                          yylval.num.is_long = 0;
                          yylval.num.is_unsigned = 0;
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 0;
                          yylval.num.is_char = 1; return NUM;
                      }
\'([^\\\n\']|\\(.|\r?\n))*\' {
                          yylval.num.val = scan_character(yytext, yyleng);
                          yylval.num.is_long = 0;
                          yylval.num.is_unsigned = 0;
                          yylval.num.is_hex = 0;
                          yylval.num.is_octal = 0;
                          yylval.num.is_char = 1; return NUM;
                      }
L?\'([^\\\n\']|\\(.|\r?\n))* {
                          diag_error_at(scan_loc(),
                                        "unterminated character constant");
                      }

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
