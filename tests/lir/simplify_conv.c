/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_simplify_conv.h"

void test_simplify_conversion_exact_pattern(void)
{
    LirFn *fn=test_fn("simplify_conv"); int b=fn->entry_block;
    int a=test_vreg(fn),c=test_vreg(fn),x=test_vreg(fn),y=test_vreg(fn),z=test_vreg(fn);
    test_emit(fn,b,(Instr){.op=LIR_LOAD,.dst=a,.a=lir_mem(LIR_FP,-4),.w=LIR_W4,.aux=4});
    test_emit(fn,b,(Instr){.op=LIR_CONV,.dst=c,.a=lir_vreg(a),.conv=CONV_SEXT32_64});
    test_emit(fn,b,(Instr){.op=LIR_LOAD,.dst=x,.a=lir_mem(LIR_FP,-8),.w=LIR_W8,.aux=8});
    test_emit(fn,b,(Instr){.op=LIR_CONV,.dst=y,.a=lir_vreg(x),.conv=CONV_SEXT32_64});
    test_emit(fn,b,(Instr){.op=LIR_CONV,.dst=z,.a=lir_vreg(a),.conv=CONV_ZEXT32});
    test_return(fn,b,lir_vreg(c)); test_finish(fn); CHECK(lir_simplify_conversions_function(fn)); lir_cfg_verify(fn);
    CHECK_EQ(fn->blocks[b].instrs[1].op,LIR_MOV); CHECK_EQ(fn->blocks[b].instrs[3].op,LIR_CONV); CHECK_EQ(fn->blocks[b].instrs[4].op,LIR_CONV);
}
