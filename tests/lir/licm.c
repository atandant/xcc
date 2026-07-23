/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_licm.h"

typedef struct { LirFn *fn; int pre,head,body,exit; int x,y,cond; } Loop;
static Loop loop_new(const char *name)
{
    Loop l={0}; l.fn=test_fn(name); l.pre=l.fn->entry_block; l.head=test_block(l.fn); l.body=test_block(l.fn); l.exit=test_block(l.fn);
    l.x=test_vreg(l.fn); l.y=test_vreg(l.fn); l.cond=test_vreg(l.fn);
    test_emit(l.fn,l.pre,(Instr){.op=LIR_MOVI,.dst=l.x,.a=lir_imm(3)}); test_emit(l.fn,l.pre,(Instr){.op=LIR_MOVI,.dst=l.y,.a=lir_imm(4)});
    test_emit(l.fn,l.pre,(Instr){.op=LIR_MOVI,.dst=l.cond,.a=lir_imm(1)}); test_jump(l.fn,l.pre,l.head);
    test_branch(l.fn,l.head,lir_vreg(l.cond),lir_imm(0),l.body,l.exit); test_jump(l.fn,l.body,l.head); test_return(l.fn,l.exit,lir_imm(0)); return l;
}
static Instr bin(LirOp op,int d,Operand a,Operand b)
{ return (Instr){.op=op,.dst=d,.a=a,.b=b,.w=LIR_W4,.sgn=LIR_SGN_S}; }

void test_licm_simple(void)
{
    Loop l=loop_new("licm_simple"); int v=test_vreg(l.fn); test_emit(l.fn,l.body,bin(LIR_MUL,v,lir_vreg(l.x),lir_vreg(l.y)));
    test_finish(l.fn); CHECK(lir_licm_function(l.fn)); lir_cfg_verify(l.fn); CHECK_EQ(test_def_block(l.fn,v),l.pre); CHECK_EQ(test_block_op_count(l.fn,l.body,LIR_MUL),0);
}
void test_licm_transitive(void)
{
    Loop l=loop_new("licm_transitive"); int a=test_vreg(l.fn),m=test_vreg(l.fn);
    test_emit(l.fn,l.body,bin(LIR_ADD,a,lir_vreg(l.x),lir_vreg(l.y))); test_emit(l.fn,l.body,bin(LIR_MUL,m,lir_vreg(a),lir_imm(4)));
    test_finish(l.fn); CHECK(lir_licm_function(l.fn)); lir_cfg_verify(l.fn); CHECK_EQ(test_def_block(l.fn,a),l.pre); CHECK_EQ(test_def_block(l.fn,m),l.pre);
}
void test_licm_variant(void)
{
    Loop l=loop_new("licm_variant"); int iter=test_vreg(l.fn),varying=test_vreg(l.fn),m=test_vreg(l.fn);
    LirPhi *phi=lir_block_add_phi(&l.fn->blocks[l.head],iter); lir_phi_add_input(phi,l.pre,l.x); lir_phi_add_input(phi,l.body,varying);
    test_emit(l.fn,l.body,bin(LIR_ADD,varying,lir_vreg(iter),lir_imm(1))); test_emit(l.fn,l.body,bin(LIR_MUL,m,lir_vreg(varying),lir_imm(4)));
    test_finish(l.fn); CHECK(!lir_licm_function(l.fn)); lir_cfg_verify(l.fn); CHECK_EQ(test_def_block(l.fn,varying),l.body); CHECK_EQ(test_def_block(l.fn,m),l.body);
}
void test_licm_unsafe_and_no_preheader(void)
{
    Loop l=loop_new("licm_unsafe"); int d=test_vreg(l.fn),load=test_vreg(l.fn);
    test_emit(l.fn,l.body,bin(LIR_DIV,d,lir_vreg(l.x),lir_vreg(l.y))); test_emit(l.fn,l.body,(Instr){.op=LIR_LOAD,.dst=load,.a=lir_mem(l.x,0),.aux=4});
    test_finish(l.fn); CHECK(!lir_licm_function(l.fn)); lir_cfg_verify(l.fn); CHECK_EQ(test_def_block(l.fn,d),l.body); CHECK_EQ(test_def_block(l.fn,load),l.body);

    LirFn *fn=test_fn("licm_no_preheader"); int entry=fn->entry_block,other=test_block(fn),head=test_block(fn),body=test_block(fn),exit=test_block(fn);
    int cond=test_vreg(fn),x=test_vreg(fn),v=test_vreg(fn); test_emit(fn,entry,(Instr){.op=LIR_MOVI,.dst=x,.a=lir_imm(2)});
    test_branch(fn,entry,lir_vreg(cond),lir_imm(0),head,other); test_jump(fn,other,head); test_branch(fn,head,lir_vreg(cond),lir_imm(0),body,exit);
    test_emit(fn,body,bin(LIR_MUL,v,lir_vreg(x),lir_imm(4))); test_jump(fn,body,head); test_return(fn,exit,lir_imm(0));
    test_finish(fn); CHECK(!lir_licm_function(fn)); lir_cfg_verify(fn); CHECK_EQ(test_def_block(fn,v),body);
}
