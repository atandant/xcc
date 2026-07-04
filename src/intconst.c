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

    if (!type_is_unsigned(ty))
        return int_const_binop(op, a, b, out);

    ua = truncate_unsigned((unsigned long)a, ty);
    ub = truncate_unsigned((unsigned long)b, ty);
    if (!uint_const_binop(op, ua, ub, &ur))
        return 0;
    *out = (long)truncate_unsigned(ur, ty);
    return 1;
}

/* UNDEFER: reject unsigned constant overflow past unsigned long range. */
