/* SPDX-License-Identifier: MIT */
#ifndef XCC_AST_CONST_PROP_H
#define XCC_AST_CONST_PROP_H

#include "ast.h"

/* Propagate known integer locals through fn's body. Returns 1 if changed. */
int ast_const_prop_function(Function *fn);

#endif /* XCC_AST_CONST_PROP_H */
