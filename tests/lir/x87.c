/* SPDX-License-Identifier: MIT */
#include "test.h"

#include "emit_x86.h"
#include "liveness.h"
#include "regalloc.h"
#include "type.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long double_bits(double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (long)bits;
}

static double double_from_bits(uint64_t bits)
{
    double value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int emit_binary(FILE *out, const char *name, LirOp op,
                       double left, double right)
{
    LirFn *lf = lir_fn_new(name);
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int a64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int b64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int a80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int b80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    Function fn = { .name = (char *)name, .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_double() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = a64, .a = lir_imm(double_bits(left)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = b64, .a = lir_imm(double_bits(right)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = a80, .a = lir_vreg(a64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = b80, .a = lir_vreg(b64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = op, .dst = result80, .a = lir_vreg(a80),
        .b = lir_vreg(b80), .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result64, .a = lir_vreg(result80),
        .conv = CONV_F80_F64, .fpw = LIR_FP_F64,
    });
    lir_precolor_vreg(lf, result64, PHYS_XMM0);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    for (int v = 0; v < lf->nvreg; v++) {
        if (lir_vreg_type(lf, v) != LIR_TYPE_F80)
            continue;
        if (alloc.vreg_reg[v] != REG_NONE || alloc.vreg_off[v] >= 0 ||
            (-alloc.vreg_off[v] & 15) != 0)
            return 0;
    }
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_neg(FILE *out)
{
    LirFn *lf = lir_fn_new("x87_neg");
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int source64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int source80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    Function fn = { .name = "x87_neg", .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_double() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = source64, .a = lir_imm(double_bits(10.0)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = source80, .a = lir_vreg(source64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FNEG, .dst = result80, .a = lir_vreg(source80),
        .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result64, .a = lir_vreg(result80),
        .conv = CONV_F80_F64, .fpw = LIR_FP_F64,
    });
    lir_precolor_vreg(lf, result64, PHYS_XMM0);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_precision(FILE *out)
{
    LirFn *lf = lir_fn_new("x87_precision");
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int large64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int one64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int large80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int one80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int sum80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int difference80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    Function fn = { .name = "x87_precision", .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_double() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = large64,
        .a = lir_imm(double_bits(9007199254740992.0)), .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = one64, .a = lir_imm(double_bits(1.0)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = large80, .a = lir_vreg(large64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = one80, .a = lir_vreg(one64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FADD, .dst = sum80, .a = lir_vreg(large80),
        .b = lir_vreg(one80), .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FSUB, .dst = difference80, .a = lir_vreg(sum80),
        .b = lir_vreg(large80), .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result64, .a = lir_vreg(difference80),
        .conv = CONV_F80_F64, .fpw = LIR_FP_F64,
    });
    lir_precolor_vreg(lf, result64, PHYS_XMM0);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_compare(FILE *out, const char *name, LirCond cc,
                        double left, double right)
{
    LirFn *lf = lir_fn_new(name);
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int a64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int b64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int a80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int b80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result = lir_new_vreg_type(lf, LIR_TYPE_I32);
    Function fn = { .name = (char *)name, .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_int() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = a64, .a = lir_imm(double_bits(left)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = b64, .a = lir_imm(double_bits(right)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = a80, .a = lir_vreg(a64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = b80, .a = lir_vreg(b64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FSETCC, .dst = result, .a = lir_vreg(a80),
        .b = lir_vreg(b80), .fpw = LIR_FP_F80, .cc = cc,
    });
    lir_precolor_vreg(lf, result, PHYS_RAX);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_memory_roundtrip(FILE *out)
{
    LirFn *lf = lir_fn_new("x87_memory_roundtrip");
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int address = lir_new_vreg_type(lf, LIR_TYPE_I64);
    int source64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int source80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int loaded80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int copied80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    Function fn = { .name = "x87_memory_roundtrip",
                    .linkage = LINKAGE_EXTERNAL, .ret_ty = type_double() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_LEA_SYM, .dst = address, .sym_name = "x87_slot",
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = source64, .a = lir_imm(double_bits(6.25)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = source80, .a = lir_vreg(source64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_STORE, .a = lir_mem(address, 0), .b = lir_vreg(source80),
        .aux = 16, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_LOAD, .dst = loaded80, .a = lir_mem(address, 0),
        .aux = 16, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_MOV, .dst = copied80, .a = lir_vreg(loaded80),
        .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result64, .a = lir_vreg(copied80),
        .conv = CONV_F80_F64, .fpw = LIR_FP_F64,
    });
    lir_precolor_vreg(lf, result64, PHYS_XMM0);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_abi_call(FILE *out)
{
    LirFn *lf = lir_fn_new("x87_abi_call");
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int a64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int b64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int a80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int b80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int call_result = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    Operand args[2] = { lir_vreg(a80), lir_vreg(b80) };
    LirType arg_types[2] = { LIR_TYPE_F80, LIR_TYPE_F80 };
    Function fn = { .name = "x87_abi_call", .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_double() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = a64, .a = lir_imm(double_bits(4.5)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = b64, .a = lir_imm(double_bits(2.25)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = a80, .a = lir_vreg(a64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = b80, .a = lir_vreg(b64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CALL, .dst = call_result, .call_name = "host_x87_add",
        .nargs = 2, .call_args = args, .call_arg_types = arg_types,
        .call_ret_type = LIR_TYPE_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result64, .a = lir_vreg(call_result),
        .conv = CONV_F80_F64, .fpw = LIR_FP_F64,
    });
    lir_precolor_vreg(lf, result64, PHYS_XMM0);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    if (lir_call_stack_offset(&lf->blocks[entry].instrs[4], 0) != 0 ||
        lir_call_stack_offset(&lf->blocks[entry].instrs[4], 1) != 16 ||
        lir_call_stack_size(&lf->blocks[entry].instrs[4]) != 32)
        return 0;
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_abi_return(FILE *out)
{
    LirFn *lf = lir_fn_new("x87_abi_return");
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int source64 = lir_new_vreg_type(lf, LIR_TYPE_F64);
    int result80 = lir_new_vreg_type(lf, LIR_TYPE_F80);
    Function fn = { .name = "x87_abi_return", .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_double() };
    Liveness lv;
    AllocResult alloc;

    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FMOVI, .dst = source64, .a = lir_imm(double_bits(3.5)),
        .fpw = LIR_FP_F64,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result80, .a = lir_vreg(source64),
        .conv = CONV_F64_F80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_FRET, .dst = LIR_NO_VREG, .a = lir_vreg(result80),
        .fpw = LIR_FP_F80,
    });
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

static int emit_integer_roundtrip(FILE *out, const char *name, uint64_t value,
                                  ConvKind to_f80, ConvKind from_f80)
{
    LirFn *lf = lir_fn_new(name);
    int entry = lf->entry_block;
    int epilogue = lir_new_block(lf);
    int source_i32 = to_f80 == CONV_SI32_F80 || to_f80 == CONV_UI32_F80;
    int result_i32 = from_f80 == CONV_F80_SI32 ||
                     from_f80 == CONV_F80_UI32;
    int source = lir_new_vreg_type(lf, source_i32
        ? LIR_TYPE_I32 : LIR_TYPE_I64);
    int extended = lir_new_vreg_type(lf, LIR_TYPE_F80);
    int result = lir_new_vreg_type(lf, result_i32
        ? LIR_TYPE_I32 : LIR_TYPE_I64);
    int return_value = result;
    Function fn = { .name = (char *)name, .linkage = LINKAGE_EXTERNAL,
                    .ret_ty = type_unsigned_long() };
    Liveness lv;
    AllocResult alloc;
    long bits;

    memcpy(&bits, &value, sizeof(bits));
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_MOVI, .dst = source, .a = lir_imm(bits),
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = extended, .a = lir_vreg(source),
        .conv = to_f80, .fpw = LIR_FP_F80,
    });
    lir_block_emit(&lf->blocks[entry], (Instr){
        .op = LIR_CONV, .dst = result, .a = lir_vreg(extended),
        .conv = from_f80, .fpw = LIR_FP_F80,
    });
    if (result_i32) {
        return_value = lir_new_vreg_type(lf, LIR_TYPE_I64);
        lir_block_emit(&lf->blocks[entry], (Instr){
            .op = LIR_CONV, .dst = return_value, .a = lir_vreg(result),
            .conv = from_f80 == CONV_F80_SI32
                ? CONV_SEXT32_64 : CONV_ZEXT32,
        });
    }
    lir_precolor_vreg(lf, return_value, PHYS_RAX);
    lf->blocks[entry].term = (LirTerminator){
        .kind = LIR_TERM_JMP, .target = epilogue,
    };
    lf->blocks[epilogue].term = (LirTerminator){ .kind = LIR_TERM_RET };
    lf->epilogue_label = epilogue;
    lf->stage = LIR_STAGE_LOWERED;
    lir_cfg_rebuild_preds(lf);
    lir_cfg_verify(lf);
    lir_cfg_number_instructions(lf);
    liveness_compute(lf, &X86_SYSV, &lv);
    regalloc_linear(lf, &fn, &lv, &X86_SYSV, &alloc);
    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
    emit_x86_function(lf, &fn, &alloc, out, &X86_SYSV);
    return 1;
}

void test_x87_runtime(void)
{
    const char *assembly = "build/lir-x87-generated.s";
    const char *driver = "build/lir-x87-runtime.c";
    FILE *out = fopen(assembly, "w");
    FILE *c;
    int status;

    CHECK(out != NULL);
    fputs("  .comm x87_slot,16,16\n  .text\n", out);
    CHECK(emit_binary(out, "x87_add", LIR_FADD, 10.0, 2.0));
    CHECK(emit_binary(out, "x87_sub", LIR_FSUB, 10.0, 2.0));
    CHECK(emit_binary(out, "x87_mul", LIR_FMUL, 10.0, 2.0));
    CHECK(emit_binary(out, "x87_div", LIR_FDIV, 10.0, 2.0));
    CHECK(emit_binary(out, "x87_roundtrip", LIR_FADD, 1.25, 0.0));
    CHECK(emit_neg(out));
    CHECK(emit_precision(out));
    CHECK(emit_compare(out, "x87_lt", CC_LT, 2.0, 10.0));
    CHECK(emit_compare(out, "x87_gt", CC_GT, 10.0, 2.0));
    CHECK(emit_compare(out, "x87_eq_nan", CC_EQ,
                       double_from_bits(UINT64_C(0x7ff8000000000000)), 1.0));
    CHECK(emit_compare(out, "x87_ne_nan", CC_NE,
                       double_from_bits(UINT64_C(0x7ff8000000000000)), 1.0));
    CHECK(emit_memory_roundtrip(out));
    CHECK(emit_abi_call(out));
    CHECK(emit_abi_return(out));
    CHECK(emit_integer_roundtrip(out, "x87_si32_roundtrip",
          (uint64_t)(int64_t)-123456789, CONV_SI32_F80, CONV_F80_SI32));
    CHECK(emit_integer_roundtrip(out, "x87_si64_roundtrip",
          (uint64_t)(int64_t)-9007199254740993LL,
          CONV_SI64_F80, CONV_F80_SI64));
    CHECK(emit_integer_roundtrip(out, "x87_ui32_roundtrip",
          UINT64_C(4000000200), CONV_UI32_F80, CONV_F80_UI32));
    CHECK(emit_integer_roundtrip(out, "x87_ui64_high_odd",
          UINT64_C(0x8000000000000001), CONV_UI64_F80, CONV_F80_UI64));
    CHECK(emit_integer_roundtrip(out, "x87_ui64_max",
          UINT64_MAX, CONV_UI64_F80, CONV_F80_UI64));
    fputs("  .section .note.GNU-stack,\"\",@progbits\n", out);
    CHECK(fclose(out) == 0);

    c = fopen(driver, "w");
    CHECK(c != NULL);
    fputs(
        "extern double x87_add(void), x87_sub(void), x87_mul(void);\n"
        "extern double x87_div(void), x87_roundtrip(void), x87_neg(void);\n"
        "extern double x87_precision(void);\n"
        "extern int x87_lt(void), x87_gt(void);\n"
        "extern int x87_eq_nan(void), x87_ne_nan(void);\n"
        "extern double x87_memory_roundtrip(void);\n"
        "extern double x87_abi_call(void);\n"
        "extern long double x87_abi_return(void);\n"
        "extern unsigned long x87_si32_roundtrip(void), x87_si64_roundtrip(void);\n"
        "extern unsigned long x87_ui32_roundtrip(void), x87_ui64_high_odd(void);\n"
        "extern unsigned long x87_ui64_max(void);\n"
        "long double host_x87_add(long double a, long double b) { return a+b; }\n"
        "int main(void) {\n"
        "  if (x87_add() != 12.0 || x87_sub() != 8.0) return 1;\n"
        "  if (x87_mul() != 20.0 || x87_div() != 5.0) return 2;\n"
        "  if (x87_roundtrip() != 1.25 || x87_neg() != -10.0) return 3;\n"
        "  if (x87_precision() != 1.0) return 4;\n"
        "  if (!x87_lt() || !x87_gt()) return 5;\n"
        "  if (x87_eq_nan() || !x87_ne_nan()) return 6;\n"
        "  if (x87_memory_roundtrip() != 6.25) return 7;\n"
        "  if (x87_abi_call() != 6.75) return 8;\n"
        "  if (x87_abi_return() != 3.5L) return 9;\n"
        "  if (x87_si32_roundtrip() != (unsigned long)(long)-123456789) return 10;\n"
        "  if (x87_si64_roundtrip() != (unsigned long)-9007199254740993LL) return 11;\n"
        "  if (x87_ui32_roundtrip() != 4000000200UL) return 12;\n"
        "  if (x87_ui64_high_odd() != 0x8000000000000001UL) return 13;\n"
        "  if (x87_ui64_max() != 0xffffffffffffffffUL) return 14;\n"
        "  return 0;\n"
        "}\n", c);
    CHECK(fclose(c) == 0);
    status = system("cc -std=c11 -Wall -Wextra build/lir-x87-runtime.c "
                    "build/lir-x87-generated.s -o build/lir-x87-runtime && "
                    "build/lir-x87-runtime");
    CHECK_EQ(status, 0);
}
