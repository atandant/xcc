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
