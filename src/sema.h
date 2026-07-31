/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_H
#define XCC_SEMA_H

#include "ast.h"

/* Separate pass over the AST: resolves names to typed symbols, checks types
 * and lvalues, and computes each function's stack frame size. */
void sema(ExternalDecl *prog);
GlobalObject *sema_block_static_objects(void);

#endif /* XCC_SEMA_H */
