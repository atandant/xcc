/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_FRAME_H
#define XCC_SEMA_FRAME_H

#include "ast.h"

/* Compute a safe upper bound on the stack a function body needs beyond its
 * named locals:
 *   *out_max_depth  max simultaneously-live expression temps (in 8-byte slots)
 *   *out_max_out    max outgoing stack-argument bytes over all calls
 *
 * Mirrors codegen's generic (non-fast-path) lowering; fast paths only ever
 * spill fewer temps, so this is always a safe over-estimate. */
void sema_measure_frame(Node *body, int *out_max_depth, int *out_max_out);

#endif /* XCC_SEMA_FRAME_H */
