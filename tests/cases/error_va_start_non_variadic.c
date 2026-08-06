/* SPDX-License-Identifier: MIT */
/* expect-error: va_start used in a non-variadic function */
typedef struct V { unsigned int gp; unsigned int fp; void *overflow; void *save; } va_list[1];
int f(int n)
{
    va_list ap;
    __builtin_va_start(ap, n);
    return n;
}
