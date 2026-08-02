/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_mem2reg.h"

static Instr movi(int d,long n) { return (Instr){.op=LIR_MOVI,.dst=d,.a=lir_imm(n)}; }
static Instr load(int d,int off,int size) { return (Instr){.op=LIR_LOAD,.dst=d,.a=lir_mem(LIR_FP,off),.aux=size,.w=size==8?LIR_W8:LIR_W4}; }
static Instr store(int off,int size,int v) { return (Instr){.op=LIR_STORE,.dst=LIR_NO_VREG,.a=lir_mem(LIR_FP,off),.b=lir_vreg(v),.aux=size,.w=size==8?LIR_W8:LIR_W4}; }
static Instr fload(int d,int off,LirFloatWidth fpw) { int size=fpw==LIR_FP_F32?4:8; Instr ins=load(d,off,size); ins.fpw=fpw; return ins; }
static Instr fstore(int off,int v,LirFloatWidth fpw) { int size=fpw==LIR_FP_F32?4:8; Instr ins=store(off,size,v); ins.fpw=fpw; return ins; }

void test_mem2reg_straight(void)
{
    LirFn *fn=test_fn("mem2reg_straight"); int b=fn->entry_block,v=test_vreg(fn),got=test_vreg(fn);
    test_add_local(fn,-4,4,1,0); test_emit(fn,b,movi(v,7)); test_emit(fn,b,store(-4,4,v)); test_emit(fn,b,load(got,-4,4)); test_return(fn,b,lir_vreg(got));
    test_finish(fn); CHECK(lir_promote_memory_to_registers(fn)); lir_cfg_verify(fn);
    CHECK_EQ(test_op_count(fn,LIR_STORE),0); CHECK_EQ(test_op_count(fn,LIR_LOAD),1); CHECK_EQ(fn->blocks[b].term.a.u.vreg,v);
}

void test_mem2reg_diamond(void)
{
    LirFn *fn=test_fn("mem2reg_diamond"); int e=fn->entry_block,l=test_block(fn),r=test_block(fn),j=test_block(fn);
    int cond=test_vreg(fn),lv=test_vreg(fn),rv=test_vreg(fn),got=test_vreg(fn); test_add_local(fn,-4,4,1,0);
    test_branch(fn,e,lir_vreg(cond),lir_imm(0),l,r);
    test_emit(fn,l,movi(lv,1)); test_emit(fn,l,store(-4,4,lv)); test_jump(fn,l,j);
    test_emit(fn,r,movi(rv,2)); test_emit(fn,r,store(-4,4,rv)); test_jump(fn,r,j);
    test_emit(fn,j,load(got,-4,4)); test_return(fn,j,lir_vreg(got));
    test_finish(fn); CHECK(lir_promote_memory_to_registers(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[j].nphis,1); LirPhi *p=&fn->blocks[j].phis[0];
    CHECK_EQ(test_phi_input(p,l),lv); CHECK_EQ(test_phi_input(p,r),rv); CHECK_EQ(fn->blocks[j].term.a.u.vreg,p->dst); CHECK_EQ(test_op_count(fn,LIR_STORE),0);
}

void test_mem2reg_loop(void)
{
    LirFn *fn=test_fn("mem2reg_loop"); int e=fn->entry_block,h=test_block(fn),body=test_block(fn),exit=test_block(fn);
    int init=test_vreg(fn),cur=test_vreg(fn),next=test_vreg(fn),cond=test_vreg(fn); test_add_local(fn,-4,4,1,0);
    test_emit(fn,e,movi(init,0)); test_emit(fn,e,store(-4,4,init)); test_jump(fn,e,h);
    test_emit(fn,h,load(cur,-4,4)); test_branch(fn,h,lir_vreg(cond),lir_imm(0),body,exit);
    test_emit(fn,body,(Instr){.op=LIR_ADD,.dst=next,.a=lir_vreg(cur),.b=lir_imm(1),.w=LIR_W4}); test_emit(fn,body,store(-4,4,next)); test_jump(fn,body,h);
    test_return(fn,exit,lir_vreg(cur)); test_finish(fn); CHECK(lir_promote_memory_to_registers(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[h].nphis,1); LirPhi *p=&fn->blocks[h].phis[0]; CHECK_EQ(test_phi_input(p,e),init); CHECK_EQ(test_phi_input(p,body),next);
    CHECK_EQ(fn->blocks[exit].term.a.u.vreg,p->dst); CHECK_EQ(test_op_count(fn,LIR_STORE),0);
}

static void check_float_diamond(LirType type,LirFloatWidth fpw,int size)
{
    LirFn *fn=test_fn(type==LIR_TYPE_F32?"mem2reg_f32":"mem2reg_f64");
    int e=fn->entry_block,l=test_block(fn),r=test_block(fn),j=test_block(fn);
    int cond=test_vreg(fn),lv=lir_new_vreg_type(fn,type),rv=lir_new_vreg_type(fn,type),got=lir_new_vreg_type(fn,type);
    test_add_local(fn,-size,size,1,0);
    test_branch(fn,e,lir_vreg(cond),lir_imm(0),l,r);
    test_emit(fn,l,(Instr){.op=LIR_FMOVI,.dst=lv,.a=lir_imm(0),.fpw=fpw}); test_emit(fn,l,fstore(-size,lv,fpw)); test_jump(fn,l,j);
    test_emit(fn,r,(Instr){.op=LIR_FMOVI,.dst=rv,.a=lir_imm(0),.fpw=fpw}); test_emit(fn,r,fstore(-size,rv,fpw)); test_jump(fn,r,j);
    test_emit(fn,j,fload(got,-size,fpw)); test_return(fn,j,lir_vreg(got));
    test_finish(fn); CHECK(lir_promote_memory_to_registers(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[j].nphis,1); LirPhi *p=&fn->blocks[j].phis[0];
    CHECK_EQ(lir_vreg_type(fn,p->dst),type); CHECK_EQ(test_phi_input(p,l),lv); CHECK_EQ(test_phi_input(p,r),rv);
    CHECK_EQ(fn->blocks[j].term.a.u.vreg,p->dst); CHECK_EQ(test_op_count(fn,LIR_STORE),0);
}

void test_mem2reg_float(void)
{
    check_float_diamond(LIR_TYPE_F32,LIR_FP_F32,4);
    if (lir_test_failed) return;
    check_float_diamond(LIR_TYPE_F64,LIR_FP_F64,8);
}

static int accesses(const LirFn *fn,int off)
{
    int n=0; for(int b=0;b<fn->nblocks;b++) for(int i=0;i<fn->blocks[b].ninstr;i++) {
        const Instr *in=&fn->blocks[b].instrs[i]; if((in->op==LIR_LOAD||in->op==LIR_STORE)&&in->a.kind==OPND_MEM&&in->a.u.mem.base==LIR_FP&&in->a.u.mem.disp==off) n++;
    } return n;
}
void test_mem2reg_rejections(void)
{
    LirFn *fn=test_fn("mem2reg_rejections"); int b=fn->entry_block,v=test_vreg(fn),a=test_vreg(fn),n=test_vreg(fn),ok=test_vreg(fn);
    test_add_local(fn,-4,4,1,0); test_add_local(fn,-8,4,1,1); test_add_local(fn,-9,1,0,0); test_add_local(fn,-16,4,1,0);
    test_emit(fn,b,movi(v,5));
    test_emit(fn,b,store(-4,4,v)); test_emit(fn,b,load(ok,-4,4));
    test_emit(fn,b,store(-8,4,v)); test_emit(fn,b,load(a,-8,4));
    test_emit(fn,b,store(-9,1,v)); test_emit(fn,b,load(n,-9,1));
    int bad1=test_vreg(fn),bad2=test_vreg(fn); test_emit(fn,b,load(bad1,-16,4)); test_emit(fn,b,(Instr){.op=LIR_LOAD,.dst=bad2,.a=lir_mem(LIR_FP,-16),.aux=4,.w=LIR_W4,.sgn=LIR_SGN_S});
    test_return(fn,b,lir_vreg(ok)); test_finish(fn); CHECK(lir_promote_memory_to_registers(fn)); lir_cfg_verify(fn);
    CHECK_EQ(accesses(fn,-4),1); CHECK_EQ(accesses(fn,-8),2); CHECK_EQ(accesses(fn,-9),2); CHECK_EQ(accesses(fn,-16),2);
}
