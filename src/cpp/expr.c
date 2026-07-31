/* SPDX-License-Identifier: MIT */
#include "cpp/expr.h"

#include "diag.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned long u;
    int is_unsigned;
} PpValue;

typedef struct {
    const CppToken *tokens;
    size_t len;
    size_t pos;
    int failed;
} Parser;

static PpValue signed_value(long value)
{
    PpValue result = { (unsigned long)value, 0 };
    return result;
}

static PpValue unsigned_value(unsigned long value)
{
    PpValue result = { value, 1 };
    return result;
}

static long as_signed(PpValue value)
{
    return (long)value.u;
}

static int truth(PpValue value)
{
    return value.u != 0;
}

static PpValue convert_value(PpValue value, int make_unsigned)
{
    return make_unsigned ? unsigned_value(value.u)
                         : signed_value(as_signed(value));
}

static const CppToken *peek_token(Parser *parser)
{
    return parser->pos < parser->len ? &parser->tokens[parser->pos] : NULL;
}

static int token_is(const CppToken *token, const char *text)
{
    return token && token->text && strcmp(token->text, text) == 0;
}

static int match(Parser *parser, const char *text)
{
    if (!token_is(peek_token(parser), text))
        return 0;
    parser->pos++;
    return 1;
}

static SourceLoc error_loc(Parser *parser)
{
    if (parser->pos < parser->len)
        return parser->tokens[parser->pos].loc;
    if (parser->len)
        return parser->tokens[parser->len - 1].loc;
    return (SourceLoc){0};
}

static void expression_error(Parser *parser, SourceLoc loc, const char *message)
{
    if (!parser->failed)
        diag_error_at(loc, "%s", message);
    parser->failed = 1;
}

static int hex_digit(int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static int parse_integer(Parser *parser, const CppToken *token,
                         PpValue *out)
{
    size_t digits = token->len;
    int seen_u = 0;
    int seen_l = 0;
    int base = 10;
    char *spelling;
    char *end;
    unsigned long value;

    while (digits > 0 && (token->text[digits - 1] == 'u' ||
                          token->text[digits - 1] == 'U' ||
                          token->text[digits - 1] == 'l' ||
                          token->text[digits - 1] == 'L')) {
        int ch = token->text[--digits];
        if (ch == 'u' || ch == 'U')
            seen_u++;
        else
            seen_l++;
    }
    if (seen_u > 1 || seen_l > 1 || digits == 0) {
        expression_error(parser, token->loc,
                         "invalid integer constant in #if expression");
        return 0;
    }
    if (digits >= 2 && token->text[0] == '0' &&
        (token->text[1] == 'x' || token->text[1] == 'X')) {
        base = 16;
        if (digits == 2) {
            expression_error(parser, token->loc,
                             "invalid integer constant in #if expression");
            return 0;
        }
        for (size_t i = 2; i < digits; i++)
            if (hex_digit((unsigned char)token->text[i]) < 0) {
                expression_error(parser, token->loc,
                                 "invalid integer constant in #if expression");
                return 0;
            }
    } else if (digits > 1 && token->text[0] == '0') {
        base = 8;
        for (size_t i = 1; i < digits; i++)
            if (token->text[i] < '0' || token->text[i] > '7') {
                expression_error(parser, token->loc,
                                 "invalid integer constant in #if expression");
                return 0;
            }
    } else {
        for (size_t i = 0; i < digits; i++)
            if (token->text[i] < '0' || token->text[i] > '9') {
                expression_error(parser, token->loc,
                                 "invalid integer constant in #if expression");
                return 0;
            }
    }

    spelling = malloc(digits + 1);
    if (!spelling)
        diag_fatal("out of memory parsing #if expression");
    memcpy(spelling, token->text, digits);
    spelling[digits] = '\0';
    errno = 0;
    value = strtoul(spelling, &end, base);
    {
        int consumed_all = *end == '\0';
        free(spelling);
        if (errno == ERANGE || !consumed_all ||
            (!seen_u && base == 10 && value > (unsigned long)LONG_MAX)) {
            expression_error(parser, token->loc,
                             "integer constant out of range in #if expression");
            return 0;
        }
    }
    /* During C89 conditional inclusion, signed and unsigned integer types
     * act as the implementation's widest corresponding types. */
    *out = (seen_u || (base != 10 && value > (unsigned long)LONG_MAX))
         ? unsigned_value(value) : signed_value((long)value);
    return 1;
}

static int parse_escape(Parser *parser, const CppToken *token, size_t *at,
                        unsigned long *out)
{
    int ch;
    unsigned long value;
    int count;

    if (*at >= token->len - 1) {
        expression_error(parser, token->loc,
                         "invalid character constant in #if expression");
        return 0;
    }
    ch = (unsigned char)token->text[(*at)++];
    switch (ch) {
    case '\'': case '"': case '?': case '\\': *out = (unsigned long)ch; return 1;
    case 'a': *out = 7; return 1;
    case 'b': *out = 8; return 1;
    case 'f': *out = 12; return 1;
    case 'n': *out = 10; return 1;
    case 'r': *out = 13; return 1;
    case 't': *out = 9; return 1;
    case 'v': *out = 11; return 1;
    case 'x':
        value = 0;
        count = 0;
        while (*at < token->len - 1) {
            int digit = hex_digit((unsigned char)token->text[*at]);
            if (digit < 0)
                break;
            if (value > (ULONG_MAX - (unsigned long)digit) / 16)
                value = ULONG_MAX;
            else
                value = value * 16 + (unsigned long)digit;
            (*at)++;
            count++;
        }
        if (!count || value > UCHAR_MAX) {
            expression_error(parser, token->loc,
                             "invalid character constant in #if expression");
            return 0;
        }
        *out = value;
        return 1;
    default:
        if (ch < '0' || ch > '7') {
            expression_error(parser, token->loc,
                             "invalid character constant in #if expression");
            return 0;
        }
        value = (unsigned long)(ch - '0');
        count = 1;
        while (count < 3 && *at < token->len - 1 &&
               token->text[*at] >= '0' && token->text[*at] <= '7') {
            value = value * 8 + (unsigned long)(token->text[*at] - '0');
            (*at)++;
            count++;
        }
        if (value > UCHAR_MAX) {
            expression_error(parser, token->loc,
                             "invalid character constant in #if expression");
            return 0;
        }
        *out = value;
        return 1;
    }
}

static int parse_character(Parser *parser, const CppToken *token,
                           PpValue *out)
{
    size_t at = 1;
    int count = 0;
    unsigned long value = 0;

    if (token->len < 3 || token->text[0] != '\'' ||
        token->text[token->len - 1] != '\'') {
        expression_error(parser, token->loc,
                         "invalid character constant in #if expression");
        return 0;
    }
    while (at < token->len - 1) {
        unsigned long ch;
        if (token->text[at] == '\\') {
            at++;
            if (!parse_escape(parser, token, &at, &ch))
                return 0;
        } else {
            ch = (unsigned char)token->text[at++];
        }
        if (++count > 4) {
            expression_error(parser, token->loc,
                             "character constant exceeds 4 bytes in #if expression");
            return 0;
        }
        value = (value << 8) | ch;
    }
    if (!count) {
        expression_error(parser, token->loc,
                         "empty character constant in #if expression");
        return 0;
    }
    *out = signed_value((long)value);
    return 1;
}

static PpValue parse_conditional(Parser *parser, int evaluate);

static PpValue parse_primary(Parser *parser, int evaluate)
{
    const CppToken *token = peek_token(parser);
    PpValue value = signed_value(0);

    (void)evaluate;
    if (!token) {
        expression_error(parser, error_loc(parser),
                         "expected expression after conditional directive");
        return value;
    }
    if (match(parser, "(")) {
        value = parse_conditional(parser, evaluate);
        if (!match(parser, ")"))
            expression_error(parser, error_loc(parser),
                             "expected ')' in #if expression");
        return value;
    }
    parser->pos++;
    if (token->kind == CPP_IDENT)
        return signed_value(0);
    if (token->kind == CPP_NUMBER) {
        parse_integer(parser, token, &value);
        return value;
    }
    if (token->kind == CPP_CHAR) {
        if (token->text[0] == 'L') {
            expression_error(parser, token->loc,
                             "wide character constant in #if expression");
            return value;
        }
        parse_character(parser, token, &value);
        return value;
    }
    expression_error(parser, token->loc, "invalid token in #if expression");
    return value;
}

static PpValue parse_unary(Parser *parser, int evaluate)
{
    PpValue value;
    const CppToken *operator_token = peek_token(parser);

    if (match(parser, "+"))
        return parse_unary(parser, evaluate);
    if (match(parser, "-")) {
        value = parse_unary(parser, evaluate);
        if (!evaluate || parser->failed)
            return value;
        if (value.is_unsigned)
            return unsigned_value(0UL - value.u);
        if (as_signed(value) == LONG_MIN) {
            expression_error(parser, operator_token->loc,
                             "integer overflow in #if expression");
            return signed_value(0);
        }
        return signed_value(-as_signed(value));
    }
    if (match(parser, "!")) {
        value = parse_unary(parser, evaluate);
        return signed_value(evaluate ? !truth(value) : 0);
    }
    if (match(parser, "~")) {
        value = parse_unary(parser, evaluate);
        if (!evaluate)
            return value;
        return value.is_unsigned ? unsigned_value(~value.u)
                                 : signed_value(~as_signed(value));
    }
    return parse_primary(parser, evaluate);
}

static int signed_add(long a, long b, long *out)
{
    if ((b > 0 && a > LONG_MAX - b) || (b < 0 && a < LONG_MIN - b))
        return 0;
    *out = a + b;
    return 1;
}

static int signed_sub(long a, long b, long *out)
{
    if ((b < 0 && a > LONG_MAX + b) || (b > 0 && a < LONG_MIN + b))
        return 0;
    *out = a - b;
    return 1;
}

static int signed_mul(long a, long b, long *out)
{
    if (a == 0 || b == 0) {
        *out = 0;
        return 1;
    }
    if ((a == -1 && b == LONG_MIN) || (b == -1 && a == LONG_MIN))
        return 0;
    if (a > 0) {
        if ((b > 0 && a > LONG_MAX / b) || (b < 0 && b < LONG_MIN / a))
            return 0;
    } else {
        if ((b > 0 && a < LONG_MIN / b) ||
            (b < 0 && a < LONG_MAX / b))
            return 0;
    }
    *out = a * b;
    return 1;
}

static PpValue arithmetic_error(Parser *parser, SourceLoc loc,
                                const char *message, int is_unsigned)
{
    expression_error(parser, loc, message);
    return is_unsigned ? unsigned_value(0) : signed_value(0);
}

static PpValue apply_arithmetic(Parser *parser, const CppToken *op,
                                PpValue left, PpValue right, int evaluate)
{
    int use_unsigned = left.is_unsigned || right.is_unsigned;
    unsigned long a = left.u;
    unsigned long b = right.u;
    long sa = as_signed(left);
    long sb = as_signed(right);
    long result;

    if (!evaluate)
        return use_unsigned ? unsigned_value(0) : signed_value(0);
    if (use_unsigned) {
        if (token_is(op, "+")) return unsigned_value(a + b);
        if (token_is(op, "-")) return unsigned_value(a - b);
        if (token_is(op, "*")) return unsigned_value(a * b);
        if (token_is(op, "/") || token_is(op, "%")) {
            if (b == 0)
                return arithmetic_error(parser, op->loc,
                    "division by zero in #if expression", 1);
            return unsigned_value(token_is(op, "/") ? a / b : a % b);
        }
    } else {
        if (token_is(op, "+") && !signed_add(sa, sb, &result))
            return arithmetic_error(parser, op->loc,
                "integer overflow in #if expression", 0);
        if (token_is(op, "-") && !signed_sub(sa, sb, &result))
            return arithmetic_error(parser, op->loc,
                "integer overflow in #if expression", 0);
        if (token_is(op, "*") && !signed_mul(sa, sb, &result))
            return arithmetic_error(parser, op->loc,
                "integer overflow in #if expression", 0);
        if (token_is(op, "/") || token_is(op, "%")) {
            if (sb == 0)
                return arithmetic_error(parser, op->loc,
                    "division by zero in #if expression", 0);
            if (sa == LONG_MIN && sb == -1)
                return arithmetic_error(parser, op->loc,
                    "integer overflow in #if expression", 0);
            result = token_is(op, "/") ? sa / sb : sa % sb;
        }
        return signed_value(result);
    }
    return signed_value(0);
}

static PpValue parse_multiplicative(Parser *parser, int evaluate)
{
    PpValue left = parse_unary(parser, evaluate);
    const CppToken *op;

    while ((op = peek_token(parser)) != NULL &&
           (token_is(op, "*") || token_is(op, "/") || token_is(op, "%"))) {
        parser->pos++;
        left = apply_arithmetic(parser, op, left,
                                parse_unary(parser, evaluate), evaluate);
    }
    return left;
}

static PpValue parse_additive(Parser *parser, int evaluate)
{
    PpValue left = parse_multiplicative(parser, evaluate);
    const CppToken *op;

    while ((op = peek_token(parser)) != NULL &&
           (token_is(op, "+") || token_is(op, "-"))) {
        parser->pos++;
        left = apply_arithmetic(parser, op, left,
                                parse_multiplicative(parser, evaluate), evaluate);
    }
    return left;
}

static PpValue parse_shift(Parser *parser, int evaluate)
{
    PpValue left = parse_additive(parser, evaluate);
    const CppToken *op;

    while ((op = peek_token(parser)) != NULL &&
           (token_is(op, "<<") || token_is(op, ">>"))) {
        PpValue right;
        int bits = (int)(sizeof(unsigned long) * CHAR_BIT);
        parser->pos++;
        right = parse_additive(parser, evaluate);
        if (evaluate && !parser->failed) {
            if ((right.is_unsigned && right.u >= (unsigned long)bits) ||
                (!right.is_unsigned &&
                 (as_signed(right) < 0 || as_signed(right) >= bits))) {
                left = arithmetic_error(parser, op->loc,
                    "invalid shift count in #if expression", left.is_unsigned);
            } else {
                int count = (int)right.u;
                if (left.is_unsigned) {
                    left = unsigned_value(token_is(op, "<<")
                                        ? left.u << count : left.u >> count);
                } else if (token_is(op, ">>")) {
                    long value = as_signed(left);
                    left = signed_value(value < 0 ? ~(~value >> count)
                                                  : value >> count);
                } else {
                    long value = as_signed(left);
                    if (value < 0 || (count && value > (LONG_MAX >> count)))
                        left = arithmetic_error(parser, op->loc,
                            "integer overflow in #if expression", 0);
                    else
                        left = signed_value(value << count);
                }
            }
        }
    }
    return left;
}

static PpValue comparison(PpValue left, PpValue right, const CppToken *op,
                          int evaluate)
{
    int result = 0;
    int use_unsigned = left.is_unsigned || right.is_unsigned;

    if (!evaluate)
        return signed_value(0);
    if (use_unsigned) {
        if (token_is(op, "<")) result = left.u < right.u;
        else if (token_is(op, "<=")) result = left.u <= right.u;
        else if (token_is(op, ">")) result = left.u > right.u;
        else if (token_is(op, ">=")) result = left.u >= right.u;
        else if (token_is(op, "==")) result = left.u == right.u;
        else result = left.u != right.u;
    } else {
        long a = as_signed(left);
        long b = as_signed(right);
        if (token_is(op, "<")) result = a < b;
        else if (token_is(op, "<=")) result = a <= b;
        else if (token_is(op, ">")) result = a > b;
        else if (token_is(op, ">=")) result = a >= b;
        else if (token_is(op, "==")) result = a == b;
        else result = a != b;
    }
    return signed_value(result);
}

static PpValue parse_relational(Parser *parser, int evaluate)
{
    PpValue left = parse_shift(parser, evaluate);
    const CppToken *op;

    while ((op = peek_token(parser)) != NULL &&
           (token_is(op, "<") || token_is(op, "<=") ||
            token_is(op, ">") || token_is(op, ">="))) {
        parser->pos++;
        left = comparison(left, parse_shift(parser, evaluate), op, evaluate);
    }
    return left;
}

static PpValue parse_equality(Parser *parser, int evaluate)
{
    PpValue left = parse_relational(parser, evaluate);
    const CppToken *op;

    while ((op = peek_token(parser)) != NULL &&
           (token_is(op, "==") || token_is(op, "!="))) {
        parser->pos++;
        left = comparison(left, parse_relational(parser, evaluate), op,
                          evaluate);
    }
    return left;
}

static PpValue parse_bitand(Parser *parser, int evaluate)
{
    PpValue left = parse_equality(parser, evaluate);
    while (match(parser, "&")) {
        PpValue right = parse_equality(parser, evaluate);
        int uns = left.is_unsigned || right.is_unsigned;
        left = uns ? unsigned_value(evaluate ? left.u & right.u : 0)
                   : signed_value(evaluate ? as_signed(left) & as_signed(right) : 0);
    }
    return left;
}

static PpValue parse_bitxor(Parser *parser, int evaluate)
{
    PpValue left = parse_bitand(parser, evaluate);
    while (match(parser, "^")) {
        PpValue right = parse_bitand(parser, evaluate);
        int uns = left.is_unsigned || right.is_unsigned;
        left = uns ? unsigned_value(evaluate ? left.u ^ right.u : 0)
                   : signed_value(evaluate ? as_signed(left) ^ as_signed(right) : 0);
    }
    return left;
}

static PpValue parse_bitor(Parser *parser, int evaluate)
{
    PpValue left = parse_bitxor(parser, evaluate);
    while (match(parser, "|")) {
        PpValue right = parse_bitxor(parser, evaluate);
        int uns = left.is_unsigned || right.is_unsigned;
        left = uns ? unsigned_value(evaluate ? left.u | right.u : 0)
                   : signed_value(evaluate ? as_signed(left) | as_signed(right) : 0);
    }
    return left;
}

static PpValue parse_logical_and(Parser *parser, int evaluate)
{
    PpValue left = parse_bitor(parser, evaluate);
    while (match(parser, "&&")) {
        int take_right = evaluate && truth(left);
        PpValue right = parse_bitor(parser, take_right);
        left = signed_value(evaluate && truth(left) && truth(right));
    }
    return left;
}

static PpValue parse_logical_or(Parser *parser, int evaluate)
{
    PpValue left = parse_logical_and(parser, evaluate);
    while (match(parser, "||")) {
        int take_right = evaluate && !truth(left);
        PpValue right = parse_logical_and(parser, take_right);
        left = signed_value(evaluate && (truth(left) || truth(right)));
    }
    return left;
}

static PpValue parse_conditional(Parser *parser, int evaluate)
{
    PpValue condition = parse_logical_or(parser, evaluate);
    PpValue selected;
    PpValue unselected;
    int take_then;

    if (!match(parser, "?"))
        return condition;
    take_then = evaluate && truth(condition);
    selected = parse_conditional(parser, take_then);
    if (!match(parser, ":")) {
        expression_error(parser, error_loc(parser),
                         "expected ':' in #if expression");
        return signed_value(0);
    }
    unselected = parse_conditional(parser, evaluate && !truth(condition));
    if (!take_then) {
        PpValue temporary = selected;
        selected = unselected;
        unselected = temporary;
    }
    return convert_value(selected, selected.is_unsigned || unselected.is_unsigned);
}

int cpp_eval_condition(const CppToken *tokens, size_t len, int *out_true)
{
    Parser parser = { tokens, len, 0, 0 };
    PpValue result;

    if (!len) {
        SourceLoc loc = {0};
        diag_error_at(loc, "expected expression after conditional directive");
        return 0;
    }
    result = parse_conditional(&parser, 1);
    if (!parser.failed && parser.pos != parser.len)
        expression_error(&parser, parser.tokens[parser.pos].loc,
                         "extra tokens at end of #if expression");
    if (parser.failed)
        return 0;
    *out_true = truth(result);
    return 1;
}
