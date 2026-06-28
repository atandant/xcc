/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_H
#define XCC_SEMA_H

#include "ast.h"

/* Separate pass over the AST: resolves local variables to stack offsets,
 * reports undeclared/redeclared/invalid-lvalue errors, and computes the
 * function's stack frame size. */
void sema(Function *fn);

#endif /* XCC_SEMA_H */
