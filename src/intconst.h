/* SPDX-License-Identifier: MIT */
#ifndef XCC_INTCONST_H
#define XCC_INTCONST_H

#include "ast.h"
#include "type.h"

/* Overflow-safe signed-long constant arithmetic for ICE and cosfold.
 * Returns 1 on success and stores the result in *out; returns 0 when the
 * operation is undefined or the result does not fit in signed long. */

int int_const_neg(long a, long *out);
int int_const_binop(BinOp op, long a, long b, long *out);

/* Typed constant ops: honor unsigned result types and width truncation. */
int int_const_neg_ty(long a, Type *ty, long *out);
int int_const_binop_ty(BinOp op, long a, long b, Type *ty, long *out);

#endif /* XCC_INTCONST_H */
