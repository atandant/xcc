/* SPDX-License-Identifier: MIT */
/* expect: 7 */
#include <stdarg.h>

int mixed(int n, ...)
{
    va_list ap;
    long a;
    double b;
    int c;
    va_start(ap, n);
    a = va_arg(ap, long);
    b = va_arg(ap, double);
    c = va_arg(ap, int);
    va_end(ap);
    return a + (int)b + c;
}

int main(void) { return mixed(3, 1L, (float)2.5, 4); }
