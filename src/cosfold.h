/* SPDX-License-Identifier: MIT */
#ifndef XCC_COSFOLD_H
#define XCC_COSFOLD_H

#include "ast.h"

/* Rewrite *np when operands are constant; returns 1 if the tree changed. */
int cosfold_expr(Node **np);

/* Fold every expression in fn's body. Returns 1 if anything changed. */
int cosfold_function(Function *fn);

#endif /* XCC_COSFOLD_H */
