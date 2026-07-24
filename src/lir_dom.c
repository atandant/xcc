/* SPDX-License-Identifier: MIT */
#include "lir_dom.h"

#include "arena.h"

#include <string.h>

typedef struct {
    int block;
    int next;
} WalkFrame;

static int successor_count(const LirTerminator *term)
{
    if (term->kind == LIR_TERM_JMP)
        return 1;
    if (term->kind == LIR_TERM_BR)
        return term->true_target == term->false_target ? 1 : 2;
    return 0;
}

static int successor_at(const LirTerminator *term, int index)
{
    if (term->kind == LIR_TERM_JMP || index == 0)
        return term->kind == LIR_TERM_JMP ? term->target : term->true_target;
    return term->false_target;
}

static int compute_rpo(const LirFn *fn, unsigned char *reachable,
                       int *rpo, int *rpo_index)
{
    int n = fn->nblocks;
    int *postorder = arena_alloc((size_t)n * sizeof(*postorder));
    WalkFrame *stack = arena_alloc((size_t)n * sizeof(*stack));
    int depth = 0;
    int npost = 0;

    reachable[fn->entry_block] = 1;
    stack[depth++] = (WalkFrame){ fn->entry_block, 0 };
    while (depth > 0) {
        WalkFrame *frame = &stack[depth - 1];
        const LirTerminator *term = &fn->blocks[frame->block].term;
        int nsuccessors = successor_count(term);

        if (frame->next < nsuccessors) {
            int successor = successor_at(term, frame->next++);
            if (!reachable[successor]) {
                reachable[successor] = 1;
                stack[depth++] = (WalkFrame){ successor, 0 };
            }
            continue;
        }
        postorder[npost++] = frame->block;
        depth--;
    }

    for (int i = 0; i < n; i++)
        rpo_index[i] = -1;
    for (int i = 0; i < npost; i++) {
        int block = postorder[npost - i - 1];
        rpo[i] = block;
        rpo_index[block] = i;
    }
    return npost;
}

static int intersect(const int *idom, const int *rpo_index, int a, int b)
{
    while (a != b) {
        while (rpo_index[a] > rpo_index[b])
            a = idom[a];
        while (rpo_index[b] > rpo_index[a])
            b = idom[b];
    }
    return a;
}

static void list_add(LirDomList *list, int block)
{
    if (list->nblocks > 0 && list->blocks[list->nblocks - 1] == block)
        return;
    if (list->nblocks == list->cap) {
        int new_cap = list->cap ? list->cap * 2 : 4;
        int *blocks = arena_alloc((size_t)new_cap * sizeof(*blocks));

        if (list->nblocks > 0) {
            memcpy(blocks, list->blocks,
                   (size_t)list->nblocks * sizeof(*blocks));
        }
        list->blocks = blocks;
        list->cap = new_cap;
    }
    list->blocks[list->nblocks++] = block;
}

static void number_dom_tree(const LirFn *fn, LirDom *dom)
{
    int n = fn->nblocks;
    WalkFrame *stack = arena_alloc((size_t)n * sizeof(*stack));
    int depth = 0;
    int clock = 0;

    for (int b = 0; b < n; b++) {
        dom->preorder[b] = -1;
        dom->postorder[b] = -1;
    }
    dom->preorder[fn->entry_block] = clock++;
    stack[depth++] = (WalkFrame){ fn->entry_block, 0 };
    while (depth > 0) {
        WalkFrame *frame = &stack[depth - 1];
        LirDomList *children = &dom->children[frame->block];

        if (frame->next < children->nblocks) {
            int child = children->blocks[frame->next++];
            dom->preorder[child] = clock++;
            stack[depth++] = (WalkFrame){ child, 0 };
            continue;
        }
        dom->postorder[frame->block] = clock++;
        depth--;
    }
}

void lir_dom_compute(const LirFn *fn, LirDom *dom)
{
    int n = fn->nblocks;
    int *rpo = arena_alloc((size_t)n * sizeof(*rpo));
    int *rpo_index = arena_alloc((size_t)n * sizeof(*rpo_index));

    memset(dom, 0, sizeof(*dom));
    dom->nblocks = n;
    dom->reachable = arena_alloc_zeroed((size_t)n);
    dom->idom = arena_alloc((size_t)n * sizeof(*dom->idom));
    dom->children = arena_alloc_zeroed((size_t)n * sizeof(*dom->children));
    dom->frontier = arena_alloc_zeroed((size_t)n * sizeof(*dom->frontier));
    dom->preorder = arena_alloc((size_t)n * sizeof(*dom->preorder));
    dom->postorder = arena_alloc((size_t)n * sizeof(*dom->postorder));
    int nreachable = compute_rpo(fn, dom->reachable, rpo, rpo_index);

    for (int b = 0; b < n; b++)
        dom->idom[b] = -1;
    dom->idom[fn->entry_block] = fn->entry_block;

    for (;;) {
        int changed = 0;

        for (int i = 1; i < nreachable; i++) {
            int b = rpo[i];
            const LirBlock *block = &fn->blocks[b];
            int new_idom = -1;

            for (int p = 0; p < block->npreds; p++) {
                int pred = block->preds[p];

                if (!dom->reachable[pred] || dom->idom[pred] < 0)
                    continue;
                if (new_idom < 0)
                    new_idom = pred;
                else
                    new_idom = intersect(dom->idom, rpo_index,
                                         pred, new_idom);
            }
            if (dom->idom[b] != new_idom) {
                dom->idom[b] = new_idom;
                changed = 1;
            }
        }
        if (!changed)
            break;
    }

    for (int b = 0; b < n; b++) {
        if (!dom->reachable[b] || b == fn->entry_block)
            continue;
        list_add(&dom->children[dom->idom[b]], b);
    }
    number_dom_tree(fn, dom);

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
                list_add(&dom->frontier[runner], b);
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
    return dom->preorder[dominator] <= dom->preorder[block] &&
           dom->postorder[block] <= dom->postorder[dominator];
}

int lir_dom_is_child(const LirDom *dom, int parent, int child)
{
    if (parent < 0 || parent >= dom->nblocks ||
        child < 0 || child >= dom->nblocks)
        return 0;
    return child != parent && dom->idom[child] == parent;
}

int lir_dom_in_frontier(const LirDom *dom, int block, int member)
{
    if (block < 0 || block >= dom->nblocks ||
        member < 0 || member >= dom->nblocks)
        return 0;
    const LirDomList *frontier = &dom->frontier[block];
    for (int i = 0; i < frontier->nblocks; i++) {
        if (frontier->blocks[i] == member)
            return 1;
    }
    return 0;
}
