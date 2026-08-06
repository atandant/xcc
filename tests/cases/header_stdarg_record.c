/* SPDX-License-Identifier: MIT */
/* expect: 15 */
#include <stdarg.h>

struct Pair { long a; long b; };

int pair_sum(int n, ...)
{
    va_list ap;
    struct Pair p;
    va_start(ap, n);
    p = va_arg(ap, struct Pair);
    va_end(ap);
    return p.a + p.b;
}

int main(void)
{
    struct Pair p;
    p.a = 7;
    p.b = 8;
    return pair_sum(1, p);
}
