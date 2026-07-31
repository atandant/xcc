/* SPDX-License-Identifier: MIT */
#include "test.h"
#include "arena.h"
#include <stdio.h>

int main(void)
{
    LirTestCase cases[] = {
        {"algebraic-identities",test_algebraic_identities}, {"algebraic-annihilators",test_algebraic_annihilators},
        {"algebraic-safety",test_algebraic_safety}, {"strength-mul-pow2",test_strength_mul_pow2},
        {"strength-signed-divmod",test_strength_signed_divmod}, {"strength-unsigned-rejections",test_strength_unsigned_and_rejections},
        {"simplify-conversion",test_simplify_conversion_exact_pattern}, {"dce-dead-chain",test_dce_dead_chain},
        {"dce-live-phi",test_dce_live_phi}, {"dce-effect-roots",test_dce_effect_roots},
        {"licm-simple",test_licm_simple}, {"licm-transitive",test_licm_transitive},
        {"licm-variant",test_licm_variant}, {"licm-safety",test_licm_unsafe_and_no_preheader},
        {"mem2reg-straight",test_mem2reg_straight}, {"mem2reg-diamond",test_mem2reg_diamond},
        {"mem2reg-loop",test_mem2reg_loop}, {"mem2reg-rejections",test_mem2reg_rejections},
        {"cfg-simplify-forwarding",test_cfg_simplify_forwarding},
        {"cfg-simplify-merge-unreachable",test_cfg_simplify_merge_unreachable},
        {"dom-chk-sparse",test_dom_chk_sparse},
        {"x87-runtime",test_x87_runtime},
    };
    int failed=0, n=(int)(sizeof(cases)/sizeof(cases[0]));
    for(int i=0;i<n;i++) {
        lir_test_failed=0; lir_test_fn=NULL; cases[i].fn();
        if(lir_test_failed) { printf("FAIL lir/%s\n",cases[i].name); if(lir_test_fn) lir_dump_fn(lir_test_fn,stderr); failed++; }
        else printf("ok   lir/%s\n",cases[i].name);
        arena_free_all();
    }
    printf("----\n%d passed, %d failed\n",n-failed,failed);
    return failed != 0;
}
