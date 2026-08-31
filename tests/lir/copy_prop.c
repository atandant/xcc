/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_copy_prop.h"

void test_copy_prop_f80_chain(void)
{
    LirFn *fn = test_fn("copy_prop_f80_chain");
    int b = fn->entry_block;
    int source = lir_new_vreg_type(fn, LIR_TYPE_F80);
    int mid = lir_new_vreg_type(fn, LIR_TYPE_F80);
    int tail = lir_new_vreg_type(fn, LIR_TYPE_F80);
    int result = lir_new_vreg_type(fn, LIR_TYPE_I32);

    test_emit(fn, b, (Instr){
        .op = LIR_FMOVI, .dst = source, .a = lir_imm(0), .fpw = LIR_FP_F80,
    });
    test_emit(fn, b, (Instr){
        .op = LIR_MOV, .dst = mid, .a = lir_vreg(source), .fpw = LIR_FP_F80,
    });
    test_emit(fn, b, (Instr){
        .op = LIR_MOV, .dst = tail, .a = lir_vreg(mid), .fpw = LIR_FP_F80,
    });
    test_emit(fn, b, (Instr){
        .op = LIR_CONV, .dst = result, .a = lir_vreg(tail),
        .conv = CONV_F80_SI32, .fpw = LIR_FP_F80,
    });
    test_return(fn, b, lir_vreg(result));
    test_finish(fn);
    CHECK(lir_propagate_f80_copies(fn));
    lir_cfg_verify(fn);
    CHECK_EQ(test_op_count(fn, LIR_MOV), 0);
    CHECK_EQ(fn->blocks[b].ninstr, 2);
    CHECK_EQ(fn->blocks[b].instrs[1].op, LIR_CONV);
    CHECK_EQ(fn->blocks[b].instrs[1].a.u.vreg, source);
}

void test_copy_prop_f80_self_move(void)
{
    LirFn *fn = test_fn("copy_prop_f80_self_move");
    int b = fn->entry_block;
    int value = lir_new_vreg_type(fn, LIR_TYPE_F80);

    test_emit(fn, b, (Instr){
        .op = LIR_FMOVI, .dst = value, .a = lir_imm(0), .fpw = LIR_FP_F80,
    });
    test_emit(fn, b, (Instr){
        .op = LIR_MOV, .dst = value, .a = lir_vreg(value), .fpw = LIR_FP_F80,
    });
    test_return(fn, b, lir_vreg(value));
    lir_cfg_rebuild_preds(fn);
    CHECK(lir_propagate_f80_copies(fn));
    lir_cfg_verify(fn);
    CHECK_EQ(test_op_count(fn, LIR_MOV), 0);
    CHECK_EQ(fn->blocks[b].ninstr, 1);
}
