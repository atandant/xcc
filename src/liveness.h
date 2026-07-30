/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIVENESS_H
#define XCC_LIVENESS_H

#include <stdio.h>
#include "lir.h"
#include "target.h"

enum {
    LIVE_POS_USE = 1u << 0,
    LIVE_POS_DEF = 1u << 1
};

typedef struct {
    int position;
    unsigned kind;
    unsigned weight;
} LivePosition;

typedef struct {
    int start;
    int end;
} LiveRange;

typedef struct {
    int vreg;
    int start;
    int end;
    LiveRange *ranges;
    int nranges;
    int ranges_cap;
    LivePosition *positions;
    int npositions;
    int positions_cap;
    unsigned spill_weight;
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
    int *block_loop_depth;
} Liveness;

void liveness_compute(LirFn *lf, const TargetDesc *td, Liveness *out);
void liveness_dump(LirFn *lf, const Liveness *lv, const TargetDesc *td, FILE *out);

#endif /* XCC_LIVENESS_H */
