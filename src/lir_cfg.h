/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_CFG_H
#define XCC_LIR_CFG_H

#include "lir.h"

void lir_cfg_rebuild_preds(LirFn *fn);
void lir_cfg_verify(const LirFn *fn);
void lir_cfg_lower(LirFn *fn);
void lir_cfg_number_instructions(LirFn *fn);

#endif /* XCC_LIR_CFG_H */
