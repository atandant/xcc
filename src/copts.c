/* SPDX-License-Identifier: MIT */
#include "copts.h"

#include "cosfold.h"
#include "cosprop.h"

#define MAX_ROUNDS 8

void copts_optimize(Function *prog)
{
    for (Function *fn = prog; fn; fn = fn->next) {
        if (!fn->is_definition)
            continue;
        for (int round = 0; round < MAX_ROUNDS; round++) {
            int changed = 0;

            changed |= cosprop_function(fn);
            changed |= cosfold_function(fn);
            if (!changed)
                break;
        }
    }
}
