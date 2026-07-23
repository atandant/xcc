/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_strength_reduce.h"

static Instr op(LirOp k,int d,Operand a,Operand b,LirSign s)
{ return (Instr){.op=k,.dst=d,.a=a,.b=b,.w=LIR_W8,.sgn=s}; }

void test_strength_mul_pow2(void)
{
    LirFn *fn=test_fn("mul_pow2"); int b=fn->entry_block,x=test_vreg(fn);
    test_emit(fn,b,op(LIR_MUL,test_vreg(fn),lir_vreg(x),lir_imm(8),LIR_SGN_S));
    test_emit(fn,b,op(LIR_MUL,test_vreg(fn),lir_imm(16),lir_vreg(x),LIR_SGN_S));
    test_return(fn,b,lir_imm(0)); test_finish(fn); CHECK(lir_strength_reduce_function(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[b].instrs[0].op,LIR_SHL); CHECK_EQ(fn->blocks[b].instrs[0].b.u.imm,3);
    CHECK_EQ(fn->blocks[b].instrs[1].op,LIR_SHL); CHECK_EQ(fn->blocks[b].instrs[1].a.u.vreg,x); CHECK_EQ(fn->blocks[b].instrs[1].b.u.imm,4);
}

void test_strength_signed_divmod(void)
{
    LirFn *fn=test_fn("signed_divmod"); int b=fn->entry_block,x=test_vreg(fn);
    test_emit(fn,b,op(LIR_DIV,test_vreg(fn),lir_vreg(x),lir_imm(8),LIR_SGN_S));
    test_emit(fn,b,op(LIR_MOD,test_vreg(fn),lir_vreg(x),lir_imm(8),LIR_SGN_S));
    test_return(fn,b,lir_imm(0)); test_finish(fn); CHECK(lir_strength_reduce_function(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[b].instrs[0].op,LIR_SDIV_POW2); CHECK_EQ(fn->blocks[b].instrs[1].op,LIR_SMOD_POW2);
    CHECK_EQ(fn->blocks[b].instrs[0].aux,3); CHECK_EQ(fn->blocks[b].instrs[0].b.kind,OPND_NONE);
}

void test_strength_unsigned_and_rejections(void)
{
    LirFn *fn=test_fn("unsigned_reject"); int b=fn->entry_block,x=test_vreg(fn);
    test_emit(fn,b,op(LIR_DIV,test_vreg(fn),lir_vreg(x),lir_imm(16),LIR_SGN_U));
    test_emit(fn,b,op(LIR_MOD,test_vreg(fn),lir_vreg(x),lir_imm(16),LIR_SGN_U));
    test_emit(fn,b,op(LIR_DIV,test_vreg(fn),lir_vreg(x),lir_imm(6),LIR_SGN_U));
    test_emit(fn,b,op(LIR_MUL,test_vreg(fn),lir_vreg(x),lir_imm(-8),LIR_SGN_S));
    test_return(fn,b,lir_imm(0)); test_finish(fn); CHECK(lir_strength_reduce_function(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[b].instrs[0].op,LIR_UDIV_POW2); CHECK_EQ(fn->blocks[b].instrs[1].op,LIR_UMOD_POW2);
    CHECK_EQ(fn->blocks[b].instrs[2].op,LIR_DIV); CHECK_EQ(fn->blocks[b].instrs[3].op,LIR_MUL);
}
