/* SPDX-License-Identifier: MIT */
/* expect: 10 */
#include <stdarg.h>

int sum(int n, ...)
{
    va_list ap;
    int i;
    int total = 0;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

int main(void) { return sum(4, (char)1, (short)2, 3, 4); }
