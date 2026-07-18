/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_OPT_H
#define XCC_LIR_OPT_H

#include "lir.h"

/* Optimize phi-lowered LIR before liveness and register allocation. */
void lir_optimize_function(LirFn *lf);

#endif /* XCC_LIR_OPT_H */
