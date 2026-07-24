/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_DOM_H
#define XCC_LIR_DOM_H

#include "lir.h"

typedef struct {
    int *blocks;
    int nblocks;
    int cap;
} LirDomList;

typedef struct {
    int nblocks;
    unsigned char *reachable;
    int *idom;
    LirDomList *children;
    LirDomList *frontier;
    int *preorder;
    int *postorder;
} LirDom;

void lir_dom_compute(const LirFn *fn, LirDom *dom);
int lir_dom_dominates(const LirDom *dom, int dominator, int block);
int lir_dom_is_child(const LirDom *dom, int parent, int child);
int lir_dom_in_frontier(const LirDom *dom, int block, int member);

#endif /* XCC_LIR_DOM_H */
