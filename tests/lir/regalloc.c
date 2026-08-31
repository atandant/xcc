/* SPDX-License-Identifier: MIT */
#include "test.h"

#include "emit_x86.h"
#include "liveness.h"
#include "regalloc.h"

#include <stdio.h>
#include <string.h>

void test_regalloc_two_address_affinity(void)
{
    LirFn *lf = test_fn("two_address_affinity");
    int block = lf->entry_block;
    int source = test_vreg(lf);
    int result = test_vreg(lf);
    Function fn = { .name = "two_address_affinity" };
    Liveness lv;
    AllocResult alloc;

    test_emit(lf, block, (Instr){
        .op = LIR_MOVI, .dst = source, .a = lir_imm(5),
    });
    test_emit(lf, block, (Instr){
        .op = LIR_ADD, .dst = result, .a = lir_vreg(source),
        .b = lir_imm(7), .w = LIR_W8,
    });
    test_return(lf, block, lir_vreg(result));
    lf->stage = LIR_STAGE_LOWERED;
    test_finish(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);

    CHECK(alloc.vreg_reg[source] >= 0);
    CHECK_EQ(alloc.vreg_reg[source], alloc.vreg_reg[result]);
    CHECK_EQ(alloc.spilled_vregs, 0);
}

void test_emit_direct_register_store(void)
{
    LirFn *lf = test_fn("direct_register_store");
    int entry = lf->entry_block;
    int epilogue = test_block(lf);
    int address = test_vreg(lf);
    int value = test_vreg(lf);
    int regs[] = { PHYS_R8, PHYS_R9 };
    int offsets[] = { 0, 0 };
    AllocResult alloc = {
        .vreg_reg = regs, .vreg_off = offsets, .frame_size = 0,
    };
    Function fn = {
        .name = "direct_register_store", .linkage = LINKAGE_EXTERNAL,
    };
    FILE *out = tmpfile();
    char assembly[2048];
    size_t size;

    CHECK(out != NULL);
    test_emit(lf, entry, (Instr){
        .op = LIR_STORE, .a = lir_mem(address, 0), .b = lir_vreg(value),
        .w = LIR_W4, .aux = 4,
    });
    test_jump(lf, entry, epilogue);
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    test_finish(lf);

    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    rewind(out);
    size = fread(assembly, 1, sizeof(assembly) - 1, out);
    assembly[size] = '\0';
    fclose(out);

    CHECK(strstr(assembly, "  mov %r9d, 0(%r8)\n") != NULL);
    CHECK(strstr(assembly, "  mov %r9, %rax\n") == NULL);
}
