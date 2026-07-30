/* SPDX-License-Identifier: MIT */
#ifndef XCC_REGALLOC_H
#define XCC_REGALLOC_H

#include <stdio.h>
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
    int *fragment_parent;
    int live_vregs;
    int register_vregs;
    int spilled_vregs;
    int spill_slots;
    int range_reuses;
    int split_vregs;
    int split_fragments;
    int split_moves;
    int outgoing_size;
} AllocResult;

void regalloc_trivial(LirFn *lf, Function *fn, AllocResult *out);
void regalloc_linear(LirFn *lf, Function *fn, Liveness *lv,
                     const TargetDesc *td, AllocResult *out);
void regalloc_verify(const LirFn *lf, const Liveness *lv,
                     const TargetDesc *td, const AllocResult *alloc);
void regalloc_dump(const LirFn *lf, const AllocResult *alloc,
                   const TargetDesc *td, FILE *out);

#endif /* XCC_REGALLOC_H */
