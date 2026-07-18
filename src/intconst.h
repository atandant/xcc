/* SPDX-License-Identifier: MIT */
#ifndef XCC_INTCONST_H
#define XCC_INTCONST_H

#include "ast.h"
#include "type.h"

/* Overflow-safe signed-long constant arithmetic for ICE and AST folding.
 * Returns 1 on success and stores the result in *out; returns 0 when the
 * operation is undefined or the result does not fit in signed long. */

int int_const_neg(long a, long *out);
int int_const_binop(BinOp op, long a, long b, long *out);

/* Typed constant ops: honor unsigned result types and width truncation. */
int int_const_neg_ty(long a, Type *ty, long *out);
int int_const_binop_ty(BinOp op, long a, long b, Type *ty, long *out);

typedef int (*IntConstLookupFn)(const char *name, long *out_value,
                                Type **out_ty, void *ctx);
typedef int (*IntConstSizeofFn)(Node *expr, long *out_value, void *ctx);

/* Evaluate the supported C89 integral-constant-expression subset while
 * preserving target type, width, and signedness at every operation.  Name and
 * sizeof handling remain context callbacks because enum visibility and type
 * resolution belong to sema/parser clients, not arithmetic. */
int int_const_eval(Node *expr, IntConstLookupFn lookup,
                   IntConstSizeofFn eval_sizeof, void *ctx,
                   long *out_value, Type **out_ty);
int int_const_sizeof_type(Node *expr, long *out_value, void *ctx);

#endif /* XCC_INTCONST_H */
