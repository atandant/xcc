/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_ALGEBRAIC_SIMPLIFY_H
#define XCC_LIR_ALGEBRAIC_SIMPLIFY_H

#include "lir.h"

/* Simplify identity and annihilator operations. */
int lir_algebraic_simplify_function(LirFn *lf);

#endif /* XCC_LIR_ALGEBRAIC_SIMPLIFY_H */
