/* SPDX-License-Identifier: MIT */
/* expect: 15 */
#include <stdarg.h>

int stack_args(int a, int b, int c, int d, int e, int f, ...)
{
    va_list ap;
    int x;
    int y;
    va_start(ap, f);
    x = va_arg(ap, int);
    y = va_arg(ap, int);
    va_end(ap);
    return x + y;
}

int main(void) { return stack_args(1, 2, 3, 4, 5, 6, 7, 8); }
