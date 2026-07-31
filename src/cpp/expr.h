/* SPDX-License-Identifier: MIT */
#ifndef XCC_CPP_EXPR_H
#define XCC_CPP_EXPR_H

#include "cpp/cpp.h"

/* Evaluate an already-expanded C89 preprocessing conditional expression.
 * Remaining identifiers have value zero. Returns zero after diagnosing an
 * invalid expression; otherwise stores its truth value and returns one. */
int cpp_eval_condition(const CppToken *tokens, size_t len, int *out_true);

#endif /* XCC_CPP_EXPR_H */
