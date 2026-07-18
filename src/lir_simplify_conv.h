/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_SIMPLIFY_CONV_H
#define XCC_LIR_SIMPLIFY_CONV_H

#include "lir.h"

/* Remove conversions already performed by their defining load. */
int lir_simplify_conversions_function(LirFn *lf);

#endif /* XCC_LIR_SIMPLIFY_CONV_H */
