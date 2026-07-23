/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_algebraic_simplify.h"

static Instr binary(LirOp op, int dst, Operand a, Operand b, LirWidth w)
{ return (Instr){ .op=op, .dst=dst, .a=a, .b=b, .w=w }; }

void test_algebraic_identities(void)
{
    LirFn *fn=test_fn("identities"); int b=fn->entry_block, x=test_vreg(fn);
    LirOp ops[]={LIR_ADD,LIR_ADD,LIR_SUB,LIR_MUL,LIR_MUL,LIR_OR,LIR_XOR,LIR_SHL};
    Operand as[]={lir_vreg(x),lir_imm(0),lir_vreg(x),lir_vreg(x),lir_imm(1),lir_vreg(x),lir_vreg(x),lir_vreg(x)};
    Operand bs[]={lir_imm(0),lir_vreg(x),lir_imm(0),lir_imm(1),lir_vreg(x),lir_imm(0),lir_imm(0),lir_imm(0)};
    int first=fn->nvreg;
    for(int i=0;i<8;i++) test_emit(fn,b,binary(ops[i],test_vreg(fn),as[i],bs[i],LIR_W8));
    test_return(fn,b,lir_vreg(first)); test_finish(fn);
    CHECK(lir_algebraic_simplify_function(fn)); lir_cfg_verify(fn);
    for(int i=0;i<8;i++) { CHECK_EQ(fn->blocks[b].instrs[i].op,LIR_MOV); CHECK_EQ(fn->blocks[b].instrs[i].a.u.vreg,x); }
}

void test_algebraic_annihilators(void)
{
    LirFn *fn=test_fn("annihilators"); int b=fn->entry_block,x=test_vreg(fn);
    LirOp ops[]={LIR_MUL,LIR_AND,LIR_SUB,LIR_XOR,LIR_MOD};
    Operand bs[]={lir_imm(0),lir_imm(0),lir_vreg(x),lir_vreg(x),lir_imm(1)};
    for(int i=0;i<5;i++) test_emit(fn,b,binary(ops[i],test_vreg(fn),lir_vreg(x),bs[i],LIR_W8));
    test_return(fn,b,lir_imm(0)); test_finish(fn);
    CHECK(lir_algebraic_simplify_function(fn)); lir_cfg_verify(fn);
    for(int i=0;i<5;i++) { CHECK_EQ(fn->blocks[b].instrs[i].op,LIR_MOVI); CHECK_EQ(fn->blocks[b].instrs[i].a.u.imm,0); }
}

void test_algebraic_safety(void)
{
    LirFn *fn=test_fn("algebraic_safety"); int b=fn->entry_block,x=test_vreg(fn),y=test_vreg(fn);
    test_emit(fn,b,binary(LIR_SUB,test_vreg(fn),lir_vreg(x),lir_vreg(y),LIR_W8));
    test_emit(fn,b,binary(LIR_XOR,test_vreg(fn),lir_vreg(x),lir_vreg(y),LIR_W8));
    test_emit(fn,b,binary(LIR_AND,test_vreg(fn),lir_vreg(x),lir_imm(0xffffffffL),LIR_W4));
    test_emit(fn,b,binary(LIR_AND,test_vreg(fn),lir_vreg(x),lir_imm(0xffffffffL),LIR_W8));
    test_return(fn,b,lir_imm(0)); test_finish(fn);
    CHECK(lir_algebraic_simplify_function(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[b].instrs[0].op,LIR_SUB); CHECK_EQ(fn->blocks[b].instrs[1].op,LIR_XOR);
    CHECK_EQ(fn->blocks[b].instrs[2].op,LIR_MOV); CHECK_EQ(fn->blocks[b].instrs[3].op,LIR_AND);
}
