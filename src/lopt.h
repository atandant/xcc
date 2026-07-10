/* SPDX-License-Identifier: MIT */
#ifndef XCC_LOPT_H
#define XCC_LOPT_H

#include "lir.h"
#include "ast.h"

/* Run LIR optimization passes after lowering, before liveness. */
void lopt_function(LirFn *lf, Function *fn);

/* Return log2(n) when n is a positive power of two, else -1. */
int lopt_imm_pow2_log2(long n);

#endif /* XCC_LOPT_H */
