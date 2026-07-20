/* SPDX-License-Identifier: MIT */
#include "lir_dom.h"

#include "arena.h"

#include <string.h>

static void mark_reachable(const LirFn *fn, int block, unsigned char *reachable)
{
    const LirTerminator *term;

    if (block < 0 || block >= fn->nblocks || reachable[block])
        return;
    reachable[block] = 1;
    term = &fn->blocks[block].term;
    if (term->kind == LIR_TERM_JMP) {
        mark_reachable(fn, term->target, reachable);
    } else if (term->kind == LIR_TERM_BR) {
        mark_reachable(fn, term->true_target, reachable);
        mark_reachable(fn, term->false_target, reachable);
    }
}

static int dominates(const unsigned char *sets, int n, int dominator, int block)
{
    return sets[block * n + dominator] != 0;
}

void lir_dom_compute(const LirFn *fn, LirDom *dom)
{
    int n = fn->nblocks;
    unsigned char *sets = arena_alloc_zeroed((size_t)n * (size_t)n);
    unsigned char *next = arena_alloc((size_t)n);

    memset(dom, 0, sizeof(*dom));
    dom->nblocks = n;
    dom->reachable = arena_alloc_zeroed((size_t)n);
    dom->idom = arena_alloc((size_t)n * sizeof(*dom->idom));
    dom->children = arena_alloc_zeroed((size_t)n * (size_t)n);
    dom->frontier = arena_alloc_zeroed((size_t)n * (size_t)n);
    mark_reachable(fn, fn->entry_block, dom->reachable);

    for (int b = 0; b < n; b++) {
        dom->idom[b] = -1;
        if (!dom->reachable[b])
            continue;
        if (b == fn->entry_block) {
            sets[b * n + b] = 1;
        } else {
            for (int d = 0; d < n; d++)
                sets[b * n + d] = dom->reachable[d];
        }
    }

    /* Iterative dominance is intentional: xcc functions currently have small
       CFGs, and this implementation is easier to audit.  If profiling shows
       this becoming significant, consider replacing only immediate-dominator
       calculation with Lengauer-Tarjan. */
    for (;;) {
        int changed = 0;

        for (int b = 0; b < n; b++) {
            const LirBlock *block;
            int first = 1;

            if (!dom->reachable[b] || b == fn->entry_block)
                continue;
            block = &fn->blocks[b];
            memset(next, 0, (size_t)n);
            for (int p = 0; p < block->npreds; p++) {
                int pred = block->preds[p];

                if (!dom->reachable[pred])
                    continue;
                if (first) {
                    memcpy(next, &sets[pred * n], (size_t)n);
                    first = 0;
                } else {
                    for (int d = 0; d < n; d++)
                        next[d] &= sets[pred * n + d];
                }
            }
            next[b] = 1;
            if (memcmp(next, &sets[b * n], (size_t)n) != 0) {
                memcpy(&sets[b * n], next, (size_t)n);
                changed = 1;
            }
        }
        if (!changed)
            break;
    }

    dom->idom[fn->entry_block] = fn->entry_block;
    for (int b = 0; b < n; b++) {
        if (!dom->reachable[b] || b == fn->entry_block)
            continue;
        for (int candidate = 0; candidate < n; candidate++) {
            int immediate = 1;

            if (candidate == b || !dominates(sets, n, candidate, b))
                continue;
            for (int other = 0; other < n; other++) {
                if (other == b || other == candidate ||
                    !dominates(sets, n, other, b))
                    continue;
                if (!dominates(sets, n, other, candidate)) {
                    immediate = 0;
                    break;
                }
            }
            if (immediate) {
                dom->idom[b] = candidate;
                dom->children[candidate * n + b] = 1;
                break;
            }
        }
    }

    for (int b = 0; b < n; b++) {
        const LirBlock *block;
        int reachable_preds = 0;

        if (!dom->reachable[b])
            continue;
        block = &fn->blocks[b];
        for (int p = 0; p < block->npreds; p++)
            reachable_preds += dom->reachable[block->preds[p]] != 0;
        if (reachable_preds < 2)
            continue;
        for (int p = 0; p < block->npreds; p++) {
            int runner = block->preds[p];

            if (!dom->reachable[runner])
                continue;
            while (runner != dom->idom[b]) {
                dom->frontier[runner * n + b] = 1;
                if (runner == dom->idom[runner] || dom->idom[runner] < 0)
                    break;
                runner = dom->idom[runner];
            }
        }
    }
}

int lir_dom_dominates(const LirDom *dom, int dominator, int block)
{
    if (dominator < 0 || dominator >= dom->nblocks ||
        block < 0 || block >= dom->nblocks ||
        !dom->reachable[dominator] || !dom->reachable[block])
        return 0;

    for (;;) {
        if (block == dominator)
            return 1;
        if (dom->idom[block] < 0 || dom->idom[block] == block)
            return 0;
        block = dom->idom[block];
    }
}

int lir_dom_is_child(const LirDom *dom, int parent, int child)
{
    return dom->children[parent * dom->nblocks + child] != 0;
}

int lir_dom_in_frontier(const LirDom *dom, int block, int member)
{
    return dom->frontier[block * dom->nblocks + member] != 0;
}
