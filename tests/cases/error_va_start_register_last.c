/* SPDX-License-Identifier: MIT */
/* expect-error: last named parameter of va_start must not be register */
typedef struct V { unsigned int gp; unsigned int fp; void *overflow; void *save; } va_list[1];
int f(register int last, ...)
{
    va_list ap;
    __builtin_va_start(ap, last);
    return last;
}
