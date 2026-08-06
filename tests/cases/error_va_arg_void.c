/* SPDX-License-Identifier: MIT */
/* expect-error: invalid type 'void' for va_arg */
typedef struct V { unsigned int gp; unsigned int fp; void *overflow; void *save; } va_list[1];
int f(int n, ...)
{
    va_list ap;
    __builtin_va_start(ap, n);
    __builtin_va_arg(ap, void);
    return 0;
}
