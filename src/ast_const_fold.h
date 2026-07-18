/* SPDX-License-Identifier: MIT */
#ifndef XCC_AST_CONST_FOLD_H
#define XCC_AST_CONST_FOLD_H

#include "ast.h"

/* Rewrite *np when operands are constant; returns 1 if the tree changed. */
int ast_const_fold_expr(Node **np);

/* Fold every expression in fn's body. Returns 1 if anything changed. */
int ast_const_fold_function(Function *fn);

#endif /* XCC_AST_CONST_FOLD_H */
