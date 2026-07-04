/* SPDX-License-Identifier: MIT */
#include "codegen.h"
#include "lower.h"
#include "regalloc.h"
#include "emit_x86.h"
#include "liveness.h"
#include "target.h"

void codegen(Function *prog, FILE *out)
{
    fprintf(out, "  .text\n");
    for (Function *fn = prog; fn; fn = fn->next) {
        if (!fn->is_definition)
            continue;

        LirFn *lf = lower_function(fn);

        Liveness lv;
        liveness_compute(lf, &X86_SYSV, &lv);

        AllocResult alloc;
        regalloc_linear(lf, fn, &lv, &X86_SYSV, &alloc);
        emit_x86_function(lf, fn, &alloc, out, &X86_SYSV);
    }
}
