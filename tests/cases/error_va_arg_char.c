/* SPDX-License-Identifier: MIT */
/* expect-error: type 'char' is promoted when passed through '...' */
typedef struct V { unsigned int gp; unsigned int fp; void *overflow; void *save; } va_list[1];
int f(int n, ...)
{
    va_list ap;
    __builtin_va_start(ap, n);
    return __builtin_va_arg(ap, char);
}
