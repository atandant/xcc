/* SPDX-License-Identifier: MIT */
/* expect: 6 */
#include <stdarg.h>

static int take_one(va_list ap)
{
    return va_arg(ap, int);
}

int forward(int n, ...)
{
    va_list ap;
    int a;
    int b;
    va_start(ap, n);
    a = take_one(ap);
    b = va_arg(ap, int);
    va_end(ap);
    return a + b;
}

int main(void) { return forward(2, 2, 4); }
