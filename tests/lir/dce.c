/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_dce.h"
#include "arena.h"

static Instr calc(LirOp op,int d,Operand a,Operand b)
{ return (Instr){.op=op,.dst=d,.a=a,.b=b,.w=LIR_W4}; }

void test_dce_dead_chain(void)
{
    LirFn *fn=test_fn("dead_chain"); int b=fn->entry_block,x=test_vreg(fn);
    int a=test_vreg(fn),m=test_vreg(fn),s=test_vreg(fn);
    test_emit(fn,b,calc(LIR_ADD,a,lir_vreg(x),lir_imm(1)));
    test_emit(fn,b,calc(LIR_MUL,m,lir_vreg(a),lir_imm(3)));
    test_emit(fn,b,calc(LIR_SUB,s,lir_vreg(m),lir_imm(2)));
    test_return(fn,b,lir_imm(9)); test_finish(fn); CHECK(lir_eliminate_dead_code(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[b].ninstr,0);
}

void test_dce_live_phi(void)
{
    LirFn *fn=test_fn("live_phi"); int entry=fn->entry_block,l=test_block(fn),r=test_block(fn),join=test_block(fn);
    int cond=test_vreg(fn),lv=test_vreg(fn),rv=test_vreg(fn),dead=test_vreg(fn),phi_v=test_vreg(fn);
    test_branch(fn,entry,lir_vreg(cond),lir_imm(0),l,r);
    test_emit(fn,l,(Instr){.op=LIR_MOVI,.dst=lv,.a=lir_imm(1)}); test_emit(fn,l,calc(LIR_ADD,dead,lir_vreg(lv),lir_imm(8))); test_jump(fn,l,join);
    test_emit(fn,r,(Instr){.op=LIR_MOVI,.dst=rv,.a=lir_imm(2)}); test_jump(fn,r,join);
    LirPhi *phi=lir_block_add_phi(&fn->blocks[join],phi_v); lir_phi_add_input(phi,l,lv); lir_phi_add_input(phi,r,rv);
    test_return(fn,join,lir_vreg(phi_v)); test_finish(fn); CHECK(lir_eliminate_dead_code(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[join].nphis,1); CHECK_EQ(test_def_block(fn,lv),l); CHECK_EQ(test_def_block(fn,rv),r); CHECK_EQ(test_def_block(fn,dead),LIR_NO_BLOCK);
}

void test_dce_effect_roots(void)
{
    LirFn *fn=test_fn("effect_roots"); int b=fn->entry_block;
    int base=test_vreg(fn),value=test_vreg(fn),callee=test_vreg(fn),dead_load=test_vreg(fn),dead_div=test_vreg(fn);
    test_emit(fn,b,(Instr){.op=LIR_MOVI,.dst=base,.a=lir_imm(100)}); test_emit(fn,b,(Instr){.op=LIR_MOVI,.dst=value,.a=lir_imm(7)});
    test_emit(fn,b,(Instr){.op=LIR_MOVI,.dst=callee,.a=lir_imm(200)});
    test_emit(fn,b,(Instr){.op=LIR_STORE,.dst=LIR_NO_VREG,.a=lir_mem(base,0),.b=lir_vreg(value),.aux=4});
    Operand *args=arena_alloc(sizeof(*args)); args[0]=lir_vreg(value);
    test_emit(fn,b,(Instr){.op=LIR_CALL,.dst=LIR_NO_VREG,.call_indirect=1,.call_reg=callee,.nargs=1,.call_args=args});
    test_emit(fn,b,(Instr){.op=LIR_LOAD,.dst=dead_load,.a=lir_mem(base,0),.aux=4});
    test_emit(fn,b,calc(LIR_DIV,dead_div,lir_vreg(value),lir_imm(2)));
    test_return(fn,b,lir_imm(0)); test_finish(fn); CHECK(lir_eliminate_dead_code(fn)); lir_cfg_verify(fn);
    CHECK_EQ(test_op_count(fn,LIR_STORE),1); CHECK_EQ(test_op_count(fn,LIR_CALL),1); CHECK_EQ(test_op_count(fn,LIR_LOAD),0); CHECK_EQ(test_op_count(fn,LIR_DIV),0);
    CHECK(test_def_block(fn,base)!=LIR_NO_BLOCK); CHECK(test_def_block(fn,value)!=LIR_NO_BLOCK); CHECK(test_def_block(fn,callee)!=LIR_NO_BLOCK);
}
