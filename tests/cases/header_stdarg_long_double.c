/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdarg.h>

int check(int n, ...)
{
    va_list ap;
    long double x;
    va_start(ap, n);
    x = va_arg(ap, long double);
    va_end(ap);
    return x != 12.5L;
}

int main(void) { return check(1, 12.5L); }
