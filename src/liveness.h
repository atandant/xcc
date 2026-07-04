/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIVENESS_H
#define XCC_LIVENESS_H

#include <stdio.h>
#include "lir.h"
#include "target.h"

typedef struct {
    int vreg;
    int start;
    int end;
} LiveInterval;

typedef struct {
    int npts;
    int cap;
    int *pts;
} PhysBlocks;

typedef struct {
    LiveInterval *by_vreg;
    LiveInterval *sorted;
    int nsorted;
    PhysBlocks phys[PHYS_COUNT];
} Liveness;

void liveness_compute(LirFn *lf, const TargetDesc *td, Liveness *out);
void liveness_dump(LirFn *lf, const Liveness *lv, const TargetDesc *td, FILE *out);

#endif /* XCC_LIVENESS_H */
