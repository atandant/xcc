/* SPDX-License-Identifier: MIT */
#include "lir_opt.h"

#include "lir_algebraic_simplify.h"
#include "lir_copy_prop.h"
#include "lir_dce.h"
#include "lir_licm.h"
#include "lir_mem2reg.h"
#include "lir_strength_reduce.h"

#define MAX_ROUNDS 4

void lir_optimize_ssa_function(LirFn *lf)
{
    lir_promote_memory_to_registers(lf);
    lir_propagate_f80_copies(lf);
    lir_eliminate_dead_code(lf);
    lir_licm_function(lf);
}

void lir_optimize_function(LirFn *lf)
{
    for (int round = 0; round < MAX_ROUNDS; round++) {
        int changed = 0;

        changed |= lir_propagate_f80_copies(lf);
        changed |= lir_algebraic_simplify_function(lf);
        changed |= lir_strength_reduce_function(lf);
        if (!changed)
            break;
    }
}
