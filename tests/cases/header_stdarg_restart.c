/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdarg.h>

int restart(int n, ...)
{
    va_list ap;
    int bad;
    va_start(ap, n);
    bad = va_arg(ap, int) != 9;
    va_end(ap);
    va_start(ap, n);
    bad += va_arg(ap, int) != 9;
    va_end(ap);
    return bad;
}

int main(void) { return restart(1, 9); }
