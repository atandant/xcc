/* SPDX-License-Identifier: MIT */
#include "ast_opt.h"

#include "ast_const_fold.h"
#include "ast_const_prop.h"

#define MAX_ROUNDS 8

void ast_optimize_program(Function *prog)
{
    for (Function *fn = prog; fn; fn = fn->next) {
        if (!fn->is_definition)
            continue;
        for (int round = 0; round < MAX_ROUNDS; round++) {
            int changed = 0;

            changed |= ast_const_prop_function(fn);
            changed |= ast_const_fold_function(fn);
            if (!changed)
                break;
        }
    }
}
