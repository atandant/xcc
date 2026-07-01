/* SPDX-License-Identifier: MIT */
#ifndef XCC_COSPROP_H
#define XCC_COSPROP_H

#include "ast.h"

/* Propagate known integer locals through fn's body. Returns 1 if changed. */
int cosprop_function(Function *fn);

#endif /* XCC_COSPROP_H */
