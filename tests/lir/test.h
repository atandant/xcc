/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_TEST_H
#define XCC_LIR_TEST_H

#include "lir.h"
#include "lir_cfg.h"

typedef void (*LirTestFn)(void);
typedef struct { const char *name; LirTestFn fn; } LirTestCase;

extern int lir_test_failed;
extern LirFn *lir_test_fn;

void lir_test_fail(const char *file, int line, const char *fmt, ...);
LirFn *test_fn(const char *name);
int test_vreg(LirFn *fn);
int test_block(LirFn *fn);
void test_emit(LirFn *fn, int block, Instr ins);
void test_jump(LirFn *fn, int from, int to);
void test_branch(LirFn *fn, int from, Operand a, Operand b, int yes, int no);
void test_return(LirFn *fn, int block, Operand value);
void test_finish(LirFn *fn);
int test_op_count(const LirFn *fn, LirOp op);
int test_block_op_count(const LirFn *fn, int block, LirOp op);
int test_def_block(const LirFn *fn, int vreg);
int test_phi_input(const LirPhi *phi, int pred);
void test_add_local(LirFn *fn, int offset, int size, int promotable,
                    int address_taken);

#define CHECK(c) do { if (!(c)) { lir_test_fail(__FILE__, __LINE__, \
    "check failed: %s", #c); return; } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { lir_test_fail(__FILE__, __LINE__, \
    "expected %s == %s, got %ld and %ld", #a, #b, _a, _b); return; } \
    } while (0)

void test_algebraic_identities(void);
void test_algebraic_annihilators(void);
void test_algebraic_safety(void);
void test_strength_mul_pow2(void);
void test_strength_signed_divmod(void);
void test_strength_unsigned_and_rejections(void);
void test_simplify_conversion_exact_pattern(void);
void test_dce_dead_chain(void);
void test_dce_live_phi(void);
void test_dce_effect_roots(void);
void test_licm_simple(void);
void test_licm_transitive(void);
void test_licm_variant(void);
void test_licm_unsafe_and_no_preheader(void);
void test_mem2reg_straight(void);
void test_mem2reg_diamond(void);
void test_mem2reg_loop(void);
void test_mem2reg_rejections(void);
void test_cfg_simplify_forwarding(void);
void test_cfg_simplify_merge_unreachable(void);

#endif
