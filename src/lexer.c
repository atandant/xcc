/* SPDX-License-Identifier: MIT */
#include "lexer.h"

#include "arena.h"
#include "ast.h"
#include "cpp/cpp.h"
#include "diag.h"
#include "parser.h"
#include "sema_typedef.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static Cpp *lexer_cpp;

void lexer_set_source(const SourceFile *source, const CppOptions *options)
{
    lexer_cpp = cpp_create(source, options);
}

static void set_location(CppToken token)
{
    yylloc.file = token.loc.file;
    yylloc.first_line = token.loc.line;
    yylloc.first_column = token.loc.col;
    yylloc.last_line = token.loc.line;
    yylloc.last_column = token.loc.col + (int)(token.len ? token.len - 1 : 0);
}

static int hex_value(int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch - 'A' + 10;
}

static StringToken *decode_quoted(CppToken token, int quote_offset)
{
    StringToken *out = arena_alloc(sizeof(*out));
    unsigned char *data = arena_alloc(token.len ? token.len : 1);
    size_t end = token.len;
    int n = 0;

    if (end <= (size_t)quote_offset + 1 || token.text[end - 1] != token.text[quote_offset]) {
        diag_error_at(token.loc, token.text[quote_offset] == '\''
                      ? "unterminated character constant"
                      : "unterminated character string literal");
        end = token.len;
    } else {
        end--;
    }

    for (size_t i = (size_t)quote_offset + 1; i < end; i++) {
        unsigned long value;
        int c = (unsigned char)token.text[i];

        if (c != '\\') {
            data[n++] = (unsigned char)c;
            continue;
        }
        if (++i >= end) {
            data[n++] = '\\';
            break;
        }
        c = (unsigned char)token.text[i];
        switch (c) {
        case '\'': data[n++] = '\''; break;
        case '"':  data[n++] = '"'; break;
        case '?':  data[n++] = '?'; break;
        case '\\': data[n++] = '\\'; break;
        case 'a':  data[n++] = '\a'; break;
        case 'b':  data[n++] = '\b'; break;
        case 'f':  data[n++] = '\f'; break;
        case 'n':  data[n++] = '\n'; break;
        case 'r':  data[n++] = '\r'; break;
        case 't':  data[n++] = '\t'; break;
        case 'v':  data[n++] = '\v'; break;
        case 'x':
            if (i + 1 >= end || !((token.text[i + 1] >= '0' && token.text[i + 1] <= '9') ||
                                  (token.text[i + 1] >= 'a' && token.text[i + 1] <= 'f') ||
                                  (token.text[i + 1] >= 'A' && token.text[i + 1] <= 'F'))) {
                diag_error_at(token.loc, "hex escape sequence has no digits");
                data[n++] = 0;
                break;
            }
            value = 0;
            while (i + 1 < end &&
                   ((token.text[i + 1] >= '0' && token.text[i + 1] <= '9') ||
                    (token.text[i + 1] >= 'a' && token.text[i + 1] <= 'f') ||
                    (token.text[i + 1] >= 'A' && token.text[i + 1] <= 'F'))) {
                int digit = hex_value((unsigned char)token.text[++i]);
                if (value <= (ULONG_MAX - (unsigned long)digit) / 16)
                    value = value * 16 + (unsigned long)digit;
                else
                    value = ULONG_MAX;
            }
            if (value > UCHAR_MAX)
                diag_error_at(token.loc, "hex escape sequence out of range");
            data[n++] = (unsigned char)value;
            break;
        default:
            if (c >= '0' && c <= '7') {
                int digits = 0;
                value = 0;
                do {
                    value = value * 8 + (unsigned long)(token.text[i] - '0');
                    i++;
                    digits++;
                } while (digits < 3 && i < end &&
                         token.text[i] >= '0' && token.text[i] <= '7');
                i--;
                if (value > UCHAR_MAX)
                    diag_error_at(token.loc, "octal escape sequence out of range");
                data[n++] = (unsigned char)value;
            } else {
                diag_error_at(token.loc, "unknown escape sequence '\\%c'", c);
                data[n++] = (unsigned char)c;
            }
            break;
        }
    }
    out->data = data;
    out->len = n;
    return out;
}

static int keyword(const char *text)
{
    static const struct { const char *name; int token; } words[] = {
        { "int", INT }, { "char", CHAR }, { "short", SHORT },
        { "long", LONG }, { "float", FLOAT }, { "double", DOUBLE },
        { "void", VOID }, { "unsigned", UNSIGNED },
        { "signed", SIGNED }, { "return", RETURN }, { "if", IF },
        { "else", ELSE }, { "while", WHILE }, { "do", DO },
        { "for", FOR }, { "switch", SWITCH }, { "case", CASE },
        { "default", DEFAULT }, { "goto", GOTO }, { "break", BREAK },
        { "continue", CONTINUE }, { "typedef", TYPEDEF },
        { "extern", EXTERN }, { "static", STATIC }, { "auto", AUTO },
        { "register", REGISTER }, { "const", CONST },
        { "volatile", VOLATILE },
        { "struct", STRUCT }, { "union", UNION },
        { "enum", ENUM }, { "sizeof", SIZEOF },
        { "__builtin_va_start", BUILTIN_VA_START },
        { "__builtin_va_arg", BUILTIN_VA_ARG },
        { "__builtin_va_end", BUILTIN_VA_END },
    };

    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        if (strcmp(text, words[i].name) == 0)
            return words[i].token;
    return 0;
}

static int punctuator(CppToken token)
{
    static const struct { const char *text; int token; } puncts[] = {
        { "...", ELLIPSIS },
        { "<<=", SHL_ASSIGN }, { ">>=", SHR_ASSIGN },
        { "*=", MUL_ASSIGN }, { "/=", DIV_ASSIGN },
        { "%=", MOD_ASSIGN }, { "+=", ADD_ASSIGN },
        { "-=", SUB_ASSIGN }, { "&=", AND_ASSIGN },
        { "^=", XOR_ASSIGN }, { "|=", OR_ASSIGN },
        { "++", INC }, { "--", DEC }, { "->", ARROW }, { "&&", LAND },
        { "||", LOR }, { "<<", SHL }, { ">>", SHR }, { "==", EQ },
        { "!=", NE }, { "<=", LE }, { ">=", GE },
    };
    const char *single = "-+*/%=<>(){};,&|^~.:?![]";

    if (token.len == 1 && strchr(single, token.text[0]))
        return (unsigned char)token.text[0];
    for (size_t i = 0; i < sizeof(puncts) / sizeof(puncts[0]); i++)
        if (strcmp(token.text, puncts[i].text) == 0)
            return puncts[i].token;
    diag_error_at(token.loc, "stray '%s' in program", token.text);
    return -1;
}

static void number_token(CppToken token)
{
    size_t digits = token.len;
    int seen_u = 0;
    int seen_l = 0;
    int invalid = 0;
    int base = 10;
    char *number;
    unsigned long uv;
    long value;

    /* C89 decimal floating constants contain a decimal point or exponent.
     * Hexadecimal floating constants are intentionally not recognized. */
    if (!(token.len >= 2 && token.text[0] == '0' &&
          (token.text[1] == 'x' || token.text[1] == 'X')) &&
        (strchr(token.text, '.') || strchr(token.text, 'e') || strchr(token.text, 'E'))) {
        size_t end = token.len;
        int fsuffix = 0;
        int lsuffix = 0;
        int dot = 0, exp = 0, digits_before = 0, digits_after = 0, exp_digits = 0;
        char *text;
        char *parse_end;

        if (end && (token.text[end - 1] == 'f' || token.text[end - 1] == 'F')) {
            fsuffix = 1; end--;
        } else if (end && (token.text[end - 1] == 'l' || token.text[end - 1] == 'L')) {
            lsuffix = 1; end--;
        }
        size_t i = 0;
        while (i < end && token.text[i] >= '0' && token.text[i] <= '9') { digits_before++; i++; }
        if (i < end && token.text[i] == '.') {
            dot = 1; i++;
            while (i < end && token.text[i] >= '0' && token.text[i] <= '9') { digits_after++; i++; }
        }
        if (i < end && (token.text[i] == 'e' || token.text[i] == 'E')) {
            exp = 1; i++;
            if (i < end && (token.text[i] == '+' || token.text[i] == '-')) i++;
            while (i < end && token.text[i] >= '0' && token.text[i] <= '9') { exp_digits++; i++; }
        }
        if (i != end || (!dot && !exp) || (!digits_before && !digits_after) ||
            (exp && !exp_digits))
            diag_error_at(token.loc, "invalid floating constant");
        text = arena_alloc(end + 1);
        memcpy(text, token.text, end); text[end] = '\0';
        errno = 0;
        yylval.num.float_val = fsuffix ? (long double)strtof(text, &parse_end) :
                               lsuffix ? strtold(text, &parse_end) :
                                         (long double)strtod(text, &parse_end);
        if (errno == ERANGE)
            diag_error_at(token.loc, "floating constant out of range");
        if (parse_end != text + end)
            diag_error_at(token.loc, "invalid floating constant");
        yylval.num.val = 0;
        yylval.num.is_long = yylval.num.is_unsigned = 0;
        yylval.num.is_hex = yylval.num.is_octal = yylval.num.is_char = 0;
        yylval.num.is_floating = 1;
        yylval.num.is_float_suffix = fsuffix;
        yylval.num.is_long_double_suffix = lsuffix;
        return;
    }

    while (digits > 0 && (token.text[digits - 1] == 'u' ||
                          token.text[digits - 1] == 'U' ||
                          token.text[digits - 1] == 'l' ||
                          token.text[digits - 1] == 'L')) {
        int c = token.text[--digits];
        if (c == 'u' || c == 'U') {
            if (seen_u++) invalid = 1;
        } else {
            if (seen_l++) invalid = 1;
        }
    }
    if (digits >= 2 && token.text[0] == '0' &&
        (token.text[1] == 'x' || token.text[1] == 'X')) {
        base = 16;
        if (digits == 2)
            invalid = 1;
    } else if (digits > 1 && token.text[0] == '0') {
        base = 8;
        for (size_t i = 1; i < digits; i++)
            if (token.text[i] < '0' || token.text[i] > '7') {
                diag_error_at(token.loc, "invalid digit in octal constant");
                invalid = 2;
                break;
            }
    }
    if (base == 16) {
        for (size_t i = 2; i < digits; i++) {
            int c = token.text[i];
            if (!((c >= '0' && c <= '9') ||
                  (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F')))
                invalid = 1;
        }
    } else {
        for (size_t i = 0; i < digits; i++)
            if (token.text[i] < '0' || token.text[i] > '9')
                invalid = 1;
    }
    if (invalid == 1)
        diag_error_at(token.loc, "invalid suffix on integer constant");

    number = arena_alloc(digits + 1);
    memcpy(number, token.text, digits);
    number[digits] = '\0';
    errno = 0;
    if (base == 10 && !seen_u) {
        value = strtol(number, NULL, base);
        if (errno == ERANGE)
            diag_error_at(token.loc, "integer constant out of range");
    } else {
        uv = strtoul(number, NULL, base);
        value = (long)uv;
        if (errno == ERANGE)
            diag_error_at(token.loc, "integer constant out of range");
    }
    yylval.num.val = value;
    yylval.num.is_long = seen_l;
    yylval.num.is_unsigned = seen_u;
    yylval.num.is_hex = base == 16;
    yylval.num.is_octal = base == 8;
    yylval.num.is_char = 0;
    yylval.num.float_val = 0.0;
    yylval.num.is_floating = 0;
    yylval.num.is_float_suffix = 0;
    yylval.num.is_long_double_suffix = 0;
}

int yylex(void)
{
    for (;;) {
        CppToken token = cpp_next(lexer_cpp);
        int kind;

        set_location(token);
        if (token.kind == CPP_EOF)
            return 0;
        if (token.kind == CPP_NEWLINE)
            continue;
        if (token.kind == CPP_IDENT) {
            kind = keyword(token.text);
            if (kind)
                return kind;
            yylval.str = (char *)token.text;
            return typedef_lookup(token.text) ? TYPEDEF_NAME : IDENT;
        }
        if (token.kind == CPP_NUMBER) {
            number_token(token);
            return NUM;
        }
        if (token.kind == CPP_CHAR) {
            int quote_offset = token.text[0] == 'L' ? 1 : 0;
            StringToken *decoded;
            unsigned long value = 0;

            if (quote_offset)
                diag_error_at(token.loc, "wide character constants are not yet supported");
            decoded = decode_quoted(token, quote_offset);
            if (decoded->len == 0)
                diag_error_at(token.loc, "empty character constant");
            else if (decoded->len > 4)
                diag_error_at(token.loc, "character constant exceeds 4 bytes");
            else
                for (int i = 0; i < decoded->len; i++)
                    value = (value << 8) | decoded->data[i];
            yylval.num.val = (long)value;
            yylval.num.is_long = 0;
            yylval.num.is_unsigned = 0;
            yylval.num.is_hex = 0;
            yylval.num.is_octal = 0;
            yylval.num.is_char = 1;
            yylval.num.float_val = 0.0;
            yylval.num.is_floating = 0;
            yylval.num.is_float_suffix = 0;
            yylval.num.is_long_double_suffix = 0;
            return NUM;
        }
        if (token.kind == CPP_STRING) {
            int quote_offset = token.text[0] == 'L' ? 1 : 0;
            if (quote_offset)
                diag_error_at(token.loc, "wide string literals are not yet supported");
            yylval.string = decode_quoted(token, quote_offset);
            return STRING;
        }
        if (token.kind == CPP_PUNCT) {
            kind = punctuator(token);
            if (kind >= 0)
                return kind;
        }
    }
}
