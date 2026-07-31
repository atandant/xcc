/* SPDX-License-Identifier: MIT */
#include "intconst.h"

#include <limits.h>

#if defined(__SIZEOF_INT128__)
typedef __int128 intconst_wide;
#else
typedef long intconst_wide;
#endif

static int fits_slong(intconst_wide v)
{
    return v >= (intconst_wide)LONG_MIN && v <= (intconst_wide)LONG_MAX;
}

static unsigned long width_mask(int bytes)
{
    if (bytes >= 8)
        return (unsigned long)-1;
    return (1UL << (bytes * 8)) - 1UL;
}

static unsigned long truncate_unsigned(unsigned long v, Type *ty)
{
    return v & width_mask(type_int_width(ty));
}

int int_const_neg(long a, long *out)
{
    if (!out)
        return 0;
    if (a == LONG_MIN)
        return 0;
    *out = -a;
    return 1;
}

int int_const_binop(BinOp op, long a, long b, long *out)
{
    intconst_wide wide;

    if (!out)
        return 0;

    switch (op) {
    case OP_ADD:
        wide = (intconst_wide)a + (intconst_wide)b;
        if (!fits_slong(wide))
            return 0;
        *out = (long)wide;
        return 1;
    case OP_SUB:
        wide = (intconst_wide)a - (intconst_wide)b;
        if (!fits_slong(wide))
            return 0;
        *out = (long)wide;
        return 1;
    case OP_MUL:
        wide = (intconst_wide)a * (intconst_wide)b;
        if (!fits_slong(wide))
            return 0;
        *out = (long)wide;
        return 1;
    case OP_DIV:
    case OP_MOD:
        if (b == 0)
            return 0;
        if (a == LONG_MIN && b == -1)
            return 0;
        *out = (op == OP_DIV) ? a / b : a % b;
        return 1;
    case OP_BITAND:
        *out = a & b;
        return 1;
    case OP_BITXOR:
        *out = a ^ b;
        return 1;
    case OP_BITOR:
        *out = a | b;
        return 1;
    case OP_EQ:
        *out = a == b;
        return 1;
    case OP_NE:
        *out = a != b;
        return 1;
    case OP_LT:
        *out = a < b;
        return 1;
    case OP_LE:
        *out = a <= b;
        return 1;
    case OP_GT:
        *out = a > b;
        return 1;
    case OP_GE:
        *out = a >= b;
        return 1;
    case OP_SHL:
    case OP_SHR:
    case OP_COMMA:
        return 0;
    }
    return 0;
}

static int uint_const_binop(BinOp op, unsigned long a, unsigned long b,
                            unsigned long *out)
{
    if (!out)
        return 0;

    switch (op) {
    case OP_ADD:
        *out = a + b;
        return 1;
    case OP_SUB:
        *out = a - b;
        return 1;
    case OP_MUL:
        *out = a * b;
        return 1;
    case OP_DIV:
    case OP_MOD:
        if (b == 0)
            return 0;
        *out = (op == OP_DIV) ? a / b : a % b;
        return 1;
    case OP_BITAND:
        *out = a & b;
        return 1;
    case OP_BITXOR:
        *out = a ^ b;
        return 1;
    case OP_BITOR:
        *out = a | b;
        return 1;
    case OP_EQ:
        *out = a == b;
        return 1;
    case OP_NE:
        *out = a != b;
        return 1;
    case OP_LT:
        *out = a < b;
        return 1;
    case OP_LE:
        *out = a <= b;
        return 1;
    case OP_GT:
        *out = a > b;
        return 1;
    case OP_GE:
        *out = a >= b;
        return 1;
    case OP_SHL:
    case OP_SHR:
    case OP_COMMA:
        return 0;
    }
    return 0;
}

int int_const_neg_ty(long a, Type *ty, long *out)
{
    unsigned long ur;
    unsigned long ua;

    if (!out || !ty || !type_is_integer(ty))
        return 0;

    if (type_is_unsigned(ty)) {
        ua = truncate_unsigned((unsigned long)a, ty);
        ur = truncate_unsigned(0UL - ua, ty);
        *out = (long)ur;
        return 1;
    }
    return int_const_neg(a, out);
}

int int_const_binop_ty(BinOp op, long a, long b, Type *ty, long *out)
{
    unsigned long ua;
    unsigned long ub;
    unsigned long ur;

    if (!out || !ty || !type_is_integer(ty))
        return 0;

    if (op == OP_SHL || op == OP_SHR) {
        int bits = type_int_width(ty) * 8;

        if (b < 0 || b >= bits)
            return 0;
        if (type_is_unsigned(ty)) {
            ua = truncate_unsigned((unsigned long)a, ty);
            ur = op == OP_SHL ? ua << b : ua >> b;
            *out = (long)truncate_unsigned(ur, ty);
            return 1;
        }
        a = type_convert_const(a, ty);
        if (op == OP_SHR) {
            *out = a < 0 ? ~(~a >> b) : a >> b;
            return 1;
        }
        if (a < 0)
            return 0;
        {
            intconst_wide wide = (intconst_wide)a << b;
            intconst_wide max = bits == (int)(sizeof(long) * 8)
                ? LONG_MAX : ((intconst_wide)1 << (bits - 1)) - 1;

            if (wide > max)
                return 0;
            *out = (long)wide;
            return 1;
        }
    }

    if (!type_is_unsigned(ty))
        return int_const_binop(op, a, b, out);

    ua = truncate_unsigned((unsigned long)a, ty);
    ub = truncate_unsigned((unsigned long)b, ty);
    if (!uint_const_binop(op, ua, ub, &ur))
        return 0;
    *out = (long)truncate_unsigned(ur, ty);
    return 1;
}

static int is_comparison(BinOp op)
{
    return op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_LE ||
           op == OP_GT || op == OP_GE;
}

int int_const_sizeof_type(Node *expr, long *out_value, void *ctx)
{
    Type *ty;

    (void)ctx;
    if (!expr || expr->kind != ND_SIZEOF || expr->operand || !out_value)
        return 0;
    ty = expr->cast_ty;
    if (!ty || type_is_void(ty) || ty->kind == TY_FUNC ||
        !type_is_complete(ty))
        return 0;
    *out_value = type_size(ty);
    return 1;
}

int int_const_eval(Node *expr, IntConstLookupFn lookup,
                   IntConstSizeofFn eval_sizeof, void *ctx,
                   long *out_value, Type **out_ty)
{
    long l;
    long r;
    Type *lty;
    Type *rty;
    Type *ty;

    if (!expr || !out_value)
        return 0;

    switch (expr->kind) {
    case ND_NUM:
        ty = expr->ty;
        if (!ty)
            ty = expr->is_char_constant ? type_int() :
                type_classify_integer_constant(
                    expr->val, expr->has_long_suffix,
                    expr->has_unsigned_suffix,
                    expr->is_hex_literal || expr->is_octal_literal);
        if (!type_is_integer(ty))
            return 0;
        *out_value = type_convert_const(expr->val, ty);
        if (out_ty)
            *out_ty = ty;
        return 1;
    case ND_VAR:
        if (!lookup || !lookup(expr->name, out_value, &ty, ctx))
            return 0;
        if (!ty)
            ty = type_int();
        if (!type_is_integer(ty))
            return 0;
        *out_value = type_convert_const(*out_value, ty);
        if (out_ty)
            *out_ty = ty;
        return 1;
    case ND_SIZEOF:
        if (!eval_sizeof || !eval_sizeof(expr, out_value, ctx))
            return 0;
        if (out_ty)
            *out_ty = type_unsigned_long();
        return 1;
    case ND_POS:
    case ND_NEG:
    case ND_BITNOT:
        if (!int_const_eval(expr->operand, lookup, eval_sizeof, ctx,
                            &l, &lty))
            return 0;
        ty = type_int_promote(lty);
        l = type_convert_const(l, ty);
        if (expr->kind == ND_POS)
            *out_value = l;
        else if (expr->kind == ND_BITNOT)
            *out_value = type_convert_const(~l, ty);
        else if (!int_const_neg_ty(l, ty, out_value))
            return 0;
        if (out_ty)
            *out_ty = ty;
        return 1;
    case ND_NOT:
        if (!int_const_eval(expr->operand, lookup, eval_sizeof, ctx,
                            &l, &lty))
            return 0;
        *out_value = !l;
        if (out_ty)
            *out_ty = type_int();
        return 1;
    case ND_LOGAND:
    case ND_LOGOR:
        if (!int_const_eval(expr->lhs, lookup, eval_sizeof, ctx, &l, &lty))
            return 0;
        if ((expr->kind == ND_LOGAND && !l) ||
            (expr->kind == ND_LOGOR && l)) {
            *out_value = expr->kind == ND_LOGOR;
        } else {
            if (!int_const_eval(expr->rhs, lookup, eval_sizeof, ctx, &r, &rty))
                return 0;
            *out_value = r != 0;
        }
        if (out_ty)
            *out_ty = type_int();
        return 1;
    case ND_COND:
        if (!int_const_eval(expr->cond, lookup, eval_sizeof, ctx, &l, &lty))
            return 0;
        return int_const_eval(l ? expr->then_expr : expr->else_expr,
                              lookup, eval_sizeof, ctx, out_value, out_ty);
    case ND_BINOP:
        if (expr->op == OP_COMMA)
            return 0;
        if (!int_const_eval(expr->lhs, lookup, eval_sizeof, ctx, &l, &lty) ||
            !int_const_eval(expr->rhs, lookup, eval_sizeof, ctx, &r, &rty))
            return 0;
        if (expr->op == OP_SHL || expr->op == OP_SHR) {
            ty = type_int_promote(lty);
            l = type_convert_const(l, ty);
            r = type_convert_const(r, type_int_promote(rty));
        } else {
            ty = type_arith_convert(lty, rty);
            l = type_convert_const(l, ty);
            r = type_convert_const(r, ty);
        }
        if (!int_const_binop_ty(expr->op, l, r, ty, out_value))
            return 0;
        if (out_ty)
            *out_ty = is_comparison(expr->op) ? type_int() : ty;
        return 1;
    case ND_CAST:
        ty = expr->ty ? expr->ty : expr->cast_ty;
        if (!type_is_integer(ty) ||
            !int_const_eval(expr->operand, lookup, eval_sizeof, ctx,
                            &l, &lty))
            return 0;
        (void)lty;
        *out_value = type_convert_const(l, ty);
        if (out_ty)
            *out_ty = ty;
        return 1;
    default:
        return 0;
    }
}

/* UNDEFER: reject unsigned constant overflow past unsigned long range. */
