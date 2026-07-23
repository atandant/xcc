/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "arena.h"
#include "lir_cfg.h"

#include <stdarg.h>
#include <stdio.h>

int lir_test_failed;
LirFn *lir_test_fn;
const char *g_filename = "<lir-test>";

void lir_test_fail(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    lir_test_failed = 1;
    fprintf(stderr, "%s:%d: ", file, line);
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr);
}

LirFn *test_fn(const char *name)
{
    lir_test_fn = lir_fn_new(name);
    lir_test_fn->stage = LIR_STAGE_SSA;
    return lir_test_fn;
}

int test_vreg(LirFn *fn) { return lir_new_vreg(fn); }
int test_block(LirFn *fn) { return lir_new_block(fn); }
void test_emit(LirFn *fn, int block, Instr ins)
{ lir_block_emit(lir_get_block(fn, block), ins); }
void test_jump(LirFn *fn, int from, int to)
{ fn->blocks[from].term = (LirTerminator){ .kind=LIR_TERM_JMP, .target=to }; }
void test_branch(LirFn *fn, int from, Operand a, Operand b, int yes, int no)
{ fn->blocks[from].term = (LirTerminator){ .kind=LIR_TERM_BR, .a=a, .b=b,
    .true_target=yes, .false_target=no, .cc=CC_NE, .w=LIR_W4 }; }
void test_return(LirFn *fn, int block, Operand value)
{ fn->blocks[block].term = (LirTerminator){ .kind=LIR_TERM_RET, .a=value }; }
void test_finish(LirFn *fn)
{ lir_cfg_rebuild_preds(fn); lir_cfg_verify(fn); }

int test_op_count(const LirFn *fn, LirOp op)
{ int n=0; for (int b=0;b<fn->nblocks;b++) n+=test_block_op_count(fn,b,op); return n; }
int test_block_op_count(const LirFn *fn, int block, LirOp op)
{ int n=0; for(int i=0;i<fn->blocks[block].ninstr;i++) n+=fn->blocks[block].instrs[i].op==op; return n; }
int test_def_block(const LirFn *fn, int vreg)
{
    for(int b=0;b<fn->nblocks;b++) {
        for(int p=0;p<fn->blocks[b].nphis;p++) if(fn->blocks[b].phis[p].dst==vreg) return b;
        for(int i=0;i<fn->blocks[b].ninstr;i++) if(fn->blocks[b].instrs[i].dst==vreg) return b;
    }
    return LIR_NO_BLOCK;
}
int test_phi_input(const LirPhi *phi, int pred)
{ for(int i=0;i<phi->ninputs;i++) if(phi->inputs[i].pred==pred) return phi->inputs[i].value; return LIR_NO_VREG; }

void test_add_local(LirFn *fn, int offset, int size, int promotable, int address_taken)
{
    FrameLocal *locals = arena_alloc((size_t)(fn->nframe_locals + 1) * sizeof(*locals));
    for (int i=0;i<fn->nframe_locals;i++) locals[i]=fn->frame_locals[i];
    locals[fn->nframe_locals++] = (FrameLocal){ offset, size, promotable, address_taken };
    fn->frame_locals = locals;
}
