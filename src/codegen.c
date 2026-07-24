/* SPDX-License-Identifier: MIT */
#include "codegen.h"
#include "lower.h"
#include "lir_opt.h"
#include "regalloc.h"
#include "emit_x86.h"
#include "liveness.h"
#include "target.h"
#include "lir_cfg.h"

static void emit_global_object(const GlobalObject *object, FILE *out)
{
    int size = type_size(object->ty);
    int align = type_align(object->ty);

    fprintf(out, "  %s\n", object->has_init_value ? ".data" : ".bss");
    fprintf(out, "  .globl %s\n", object->name);
    fprintf(out, "  .balign %d\n", align);
    fprintf(out, "%s:\n", object->name);
    if (!object->has_init_value) {
        fprintf(out, "  .zero %d\n", size);
        return;
    }
    switch (size) {
    case 1:
        fprintf(out, "  .byte %ld\n", object->init_value);
        break;
    case 2:
        fprintf(out, "  .short %ld\n", object->init_value);
        break;
    case 4:
        fprintf(out, "  .long %ld\n", object->init_value);
        break;
    case 8:
        fprintf(out, "  .quad %ld\n", object->init_value);
        break;
    default:
        fprintf(out, "  .zero %d\n", size);
        break;
    }
}

void codegen(ExternalDecl *prog, FILE *out, int verify_lir)
{
    for (ExternalDecl *external = prog; external; external = external->next) {
        if (external->kind == EXT_OBJECT && external->object->emit)
            emit_global_object(external->object, out);
    }

    fprintf(out, "  .text\n");
    for (ExternalDecl *external = prog; external; external = external->next) {
        if (external->kind != EXT_FUNCTION)
            continue;
        Function *fn = external->function;
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
