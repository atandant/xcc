/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_STRENGTH_REDUCE_H
#define XCC_LIR_STRENGTH_REDUCE_H

#include "lir.h"

/* Strength-reduce div/mod/mul on LirFn. Returns 1 if anything changed. */
int lir_strength_reduce_function(LirFn *lf);

#endif /* XCC_LIR_STRENGTH_REDUCE_H */
