/* SPDX-License-Identifier: MIT */
#ifndef XCC_LOPTSTR_H
#define XCC_LOPTSTR_H

#include "lir.h"

/* Strength-reduce div/mod/mul on LirFn. Returns 1 if anything changed. */
int loptstr_function(LirFn *lf);

#endif /* XCC_LOPTSTR_H */
