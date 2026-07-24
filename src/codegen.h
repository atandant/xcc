/* SPDX-License-Identifier: MIT */
#ifndef XCC_CODEGEN_H
#define XCC_CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* Walks the AST and emits AT&T-syntax x86-64 assembly (System V ABI). */
void codegen(ExternalDecl *prog, FILE *out, int verify_lir);

#endif /* XCC_CODEGEN_H */
