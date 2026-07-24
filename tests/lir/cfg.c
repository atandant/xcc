/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "lir_dom.h"

void test_cfg_simplify_forwarding(void)
{
    LirFn *fn = test_fn("cfg_forwarding");
    int entry = fn->entry_block;
    int yes_forward = test_block(fn);
    int no_forward = test_block(fn);
    int yes = test_block(fn);
    int no = test_block(fn);
    int dead = test_block(fn);
    int epilogue = test_block(fn);
    int cond = test_vreg(fn);

    fn->epilogue_label = epilogue;
    test_branch(fn, entry, lir_vreg(cond), lir_imm(0),
                yes_forward, no_forward);
    test_jump(fn, yes_forward, yes);
    test_jump(fn, no_forward, no);
    test_return(fn, yes, lir_imm(7));
    test_return(fn, no, lir_imm(8));
    test_return(fn, dead, lir_imm(9));
    test_return(fn, epilogue, lir_imm(0));
    test_finish(fn);

    lir_cfg_lower(fn);
    lir_cfg_verify(fn);
    CHECK_EQ(fn->nblocks, 4);
    CHECK_EQ(fn->blocks[fn->entry_block].term.kind, LIR_TERM_BR);
    CHECK_EQ(fn->blocks[fn->blocks[fn->entry_block].term.true_target].term.kind,
             LIR_TERM_RET);
    CHECK_EQ(fn->blocks[fn->blocks[fn->entry_block].term.false_target].term.kind,
             LIR_TERM_RET);
}

void test_cfg_simplify_merge_unreachable(void)
{
    LirFn *fn = test_fn("cfg_merge");
    int entry = fn->entry_block;
    int middle = test_block(fn);
    int tail = test_block(fn);
    int dead = test_block(fn);
    int epilogue = test_block(fn);
    int value = test_vreg(fn);

    fn->epilogue_label = epilogue;
    test_emit(fn, middle, (Instr){
        .op = LIR_MOVI, .dst = value, .a = lir_imm(4) });
    test_jump(fn, entry, middle);
    test_jump(fn, middle, tail);
    test_return(fn, tail, lir_vreg(value));
    test_return(fn, dead, lir_imm(0));
    test_return(fn, epilogue, lir_imm(0));
    test_finish(fn);

    lir_cfg_lower(fn);
    lir_cfg_verify(fn);
    CHECK_EQ(fn->nblocks, 2);
    CHECK_EQ(fn->blocks[fn->entry_block].ninstr, 1);
    CHECK_EQ(fn->blocks[fn->entry_block].term.kind, LIR_TERM_RET);
}

void test_dom_chk_sparse(void)
{
    LirFn *fn = test_fn("dom_chk_sparse");
    int entry = fn->entry_block;
    int left = test_block(fn);
    int right = test_block(fn);
    int header = test_block(fn);
    int body = test_block(fn);
    int exit = test_block(fn);
    int dead = test_block(fn);
    LirDom dom;

    test_branch(fn, entry, lir_imm(1), lir_imm(0), left, right);
    test_jump(fn, left, header);
    test_jump(fn, right, header);
    test_branch(fn, header, lir_imm(1), lir_imm(0), body, exit);
    test_jump(fn, body, header);
    test_return(fn, exit, lir_imm(0));
    test_return(fn, dead, lir_imm(1));
    test_finish(fn);

    lir_dom_compute(fn, &dom);
    CHECK_EQ(dom.idom[entry], entry);
    CHECK_EQ(dom.idom[left], entry);
    CHECK_EQ(dom.idom[right], entry);
    CHECK_EQ(dom.idom[header], entry);
    CHECK_EQ(dom.idom[body], header);
    CHECK_EQ(dom.idom[exit], header);
    CHECK_EQ(dom.idom[dead], -1);
    CHECK(lir_dom_dominates(&dom, entry, exit));
    CHECK(lir_dom_dominates(&dom, header, body));
    CHECK(!lir_dom_dominates(&dom, left, header));
    CHECK(!lir_dom_dominates(&dom, entry, dead));
    CHECK(lir_dom_is_child(&dom, entry, header));
    CHECK(lir_dom_is_child(&dom, header, exit));
    CHECK(lir_dom_in_frontier(&dom, left, header));
    CHECK(lir_dom_in_frontier(&dom, right, header));
    CHECK(lir_dom_in_frontier(&dom, body, header));
    CHECK(lir_dom_in_frontier(&dom, header, header));
}
