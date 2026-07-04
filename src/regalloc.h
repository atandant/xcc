/* SPDX-License-Identifier: MIT */
#ifndef XCC_REGALLOC_H
#define XCC_REGALLOC_H

#include "lir.h"
#include "liveness.h"
#include "target.h"
#include "ast.h"

#define REG_NONE (-1)

typedef struct {
    int frame_size;
    unsigned used_callee_saved;
    int *vreg_reg;
    int *vreg_off;
} AllocResult;

void regalloc_trivial(LirFn *lf, Function *fn, AllocResult *out);
void regalloc_linear(LirFn *lf, Function *fn, const Liveness *lv,
                     const TargetDesc *td, AllocResult *out);

#endif /* XCC_REGALLOC_H */
