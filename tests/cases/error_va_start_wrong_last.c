/* SPDX-License-Identifier: MIT */
/* expect-error: second argument to va_start must be the last named parameter */
typedef struct V { unsigned int gp; unsigned int fp; void *overflow; void *save; } va_list[1];
int f(int first, int last, ...)
{
    va_list ap;
    __builtin_va_start(ap, first);
    return last;
}
