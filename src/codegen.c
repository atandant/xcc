/* SPDX-License-Identifier: MIT */
#include "codegen.h"
#include "lower.h"
#include "lir_opt.h"
#include "regalloc.h"
#include "emit_x86.h"
#include "liveness.h"
#include "target.h"
#include "lir_cfg.h"
#include "diag.h"

static void emit_string_literals(FILE *out)
{
    Node *literal = string_literals();

    if (!literal)
        return;
    fprintf(out, "  .section .rodata\n");
    for (; literal; literal = literal->string_next) {
        fprintf(out, "%s:\n", literal->string_label);
        for (int i = 0; i <= literal->string_len; i++) {
            unsigned value = i == literal->string_len ? 0 : literal->string_data[i];
            fprintf(out, "  .byte %u\n", value);
        }
    }
}

static void emit_global_object(const GlobalObject *object, FILE *out)
{
    int size = type_size(object->ty);
    int align = type_align(object->ty);
    StaticReloc *reloc = object->relocs;

    fprintf(out, "  %s\n", object->init_data ? ".data" : ".bss");
    if (object->linkage == LINKAGE_INTERNAL)
        fprintf(out, "  .local %s\n", object->name);
    else
        fprintf(out, "  .globl %s\n", object->name);
    fprintf(out, "  .balign %d\n", align);
    fprintf(out, "%s:\n", object->name);
    if (!object->init_data) {
        fprintf(out, "  .zero %d\n", size);
        return;
    }
    for (int offset = 0; offset < size;) {
        if (reloc && reloc->offset == offset) {
            if (reloc->width != 8)
                diag_fatal("internal error: unsupported static relocation width");
            fprintf(out, "  .quad %s", reloc->symbol);
            if (reloc->addend > 0)
                fprintf(out, "+%ld", reloc->addend);
            else if (reloc->addend < 0)
                fprintf(out, "%ld", reloc->addend);
            fputc('\n', out);
            offset += reloc->width;
            reloc = reloc->next;
        } else {
            fprintf(out, "  .byte %u\n", object->init_data[offset]);
            offset++;
        }
    }
}

void codegen(ExternalDecl *prog, FILE *out, int verify_lir)
{
    emit_string_literals(out);
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
        if (verify_lir)
            regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
        emit_x86_function(lf, fn, &alloc, out, &X86_SYSV);
    }
}
