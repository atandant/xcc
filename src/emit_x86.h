/* SPDX-License-Identifier: MIT */
#ifndef XCC_EMIT_X86_H
#define XCC_EMIT_X86_H

#include <stdio.h>
#include "lir.h"
#include "regalloc.h"
#include "target.h"
#include "ast.h"

void emit_x86_function(LirFn *lf, Function *fn, AllocResult *alloc,
                       FILE *out, const TargetDesc *td);

#endif /* XCC_EMIT_X86_H */
