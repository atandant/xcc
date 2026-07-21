/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_DCE_H
#define XCC_LIR_DCE_H

#include "lir.h"

int lir_eliminate_dead_code(LirFn *fn);

#endif /* XCC_LIR_DCE_H */
