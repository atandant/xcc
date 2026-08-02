/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_COPY_PROP_H
#define XCC_LIR_COPY_PROP_H

#include "lir.h"

/* Redirect single-definition F80 copies to their source and remove the moves. */
int lir_propagate_f80_copies(LirFn *lf);

#endif /* XCC_LIR_COPY_PROP_H */
