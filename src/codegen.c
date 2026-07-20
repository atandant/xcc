/* SPDX-License-Identifier: MIT */
#include "codegen.h"
#include "lower.h"
#include "lir_opt.h"
#include "regalloc.h"
#include "emit_x86.h"
#include "liveness.h"
#include "target.h"
#include "lir_cfg.h"

void codegen(Function *prog, FILE *out, int verify_lir)
{
    fprintf(out, "  .text\n");
    for (Function *fn = prog; fn; fn = fn->next) {
        if (!fn->is_definition)
            continue;

        LirFn *lf = lower_function(fn);

        lir_cfg_rebuild_preds(lf);
        if (verify_lir)
            lir_cfg_verify(lf);
        lir_optimize_ssa_function(lf);
        if (verify_lir)
            lir_cfg_verify(lf);
        lir_cfg_lower(lf);
        if (verify_lir)
            lir_cfg_verify(lf);

        lir_optimize_function(lf);
        if (verify_lir)
            lir_cfg_verify(lf);

        Liveness lv;
        liveness_compute(lf, &X86_SYSV, &lv);

        AllocResult alloc;
        regalloc_linear(lf, fn, &lv, &X86_SYSV, &alloc);
        emit_x86_function(lf, fn, &alloc, out, &X86_SYSV);
    }
}
