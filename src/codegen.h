/* SPDX-License-Identifier: MIT */
#ifndef XCC_CODEGEN_H
#define XCC_CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* Walks the AST and emits AT&T-syntax x86-64 assembly (System V ABI). */
void codegen(Function *fn, FILE *out);

#endif /* XCC_CODEGEN_H */
