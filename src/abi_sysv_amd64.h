/* SPDX-License-Identifier: MIT */
#ifndef XCC_ABI_SYSV_AMD64_H
#define XCC_ABI_SYSV_AMD64_H

#include "type.h"

/* SysV AMD64 record passing (integer-only; no SSE). C89 struct/union by-value. */

typedef enum {
    ABI_ARG_GPR,       /* 1–8 bytes in one GPR                         */
    ABI_ARG_GPR_PAIR,  /* 9–16 bytes in two GPRs                       */
    ABI_ARG_STACK,     /* >16 bytes copied to the caller's stack arg area */
} AbiArgKind;

typedef enum {
    ABI_RET_GPR,       /* 1–8 bytes in RAX                             */
    ABI_RET_GPR_PAIR,  /* 9–16 bytes in RAX + RDX                      */
    ABI_RET_SRET,      /* >16 bytes via hidden pointer in RDI          */
} AbiRetKind;

typedef struct {
    AbiArgKind kind;
    int size;
    int ngpr;          /* GPR slots consumed (1 or 2; 0 for STACK)     */
} AbiArgPlan;

typedef struct {
    AbiRetKind kind;
    int size;
} AbiRetPlan;

int abi_type_is_record_pass(Type *ty);
void abi_arg_plan(Type *ty, AbiArgPlan *out);
void abi_ret_plan(Type *ty, AbiRetPlan *out);
int abi_stack_arg_bytes(int size);

#endif /* XCC_ABI_SYSV_AMD64_H */
