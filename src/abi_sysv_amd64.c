/* SPDX-License-Identifier: MIT */
#include "abi_sysv_amd64.h"

static int align_up(int n, int align)
{
    if (align <= 1)
        return n;
    return (n + align - 1) & ~(align - 1);
}

int abi_type_is_record_pass(Type *ty)
{
    return type_is_record(ty) && type_struct_is_complete(ty);
}

void abi_arg_plan(Type *ty, AbiArgPlan *out)
{
    int sz = type_size(ty);

    out->size = sz;
    if (sz <= 8) {
        out->kind = ABI_ARG_GPR;
        out->ngpr = 1;
    } else if (sz <= 16) {
        out->kind = ABI_ARG_GPR_PAIR;
        out->ngpr = 2;
    } else {
        out->kind = ABI_ARG_STACK;
        out->ngpr = 0;
    }
}

void abi_ret_plan(Type *ty, AbiRetPlan *out)
{
    int sz = type_size(ty);

    out->size = sz;
    if (sz <= 8) {
        out->kind = ABI_RET_GPR;
    } else if (sz <= 16) {
        out->kind = ABI_RET_GPR_PAIR;
    } else {
        out->kind = ABI_RET_SRET;
    }
}

int abi_stack_arg_bytes(int size)
{
    return align_up(size, 8);
}
