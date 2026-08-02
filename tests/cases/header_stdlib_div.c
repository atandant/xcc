/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <stdlib.h>

int main(void)
{
    div_t d;
    ldiv_t ld;

    if (abs(-9) != 9)
        return 1;
    if (labs(-11L) != 11L)
        return 2;
    d = div(10, 3);
    if (d.quot != 3 || d.rem != 1)
        return 3;
    ld = ldiv(10L, 3L);
    if (ld.quot != 3L || ld.rem != 1L)
        return 4;
    return 0;
}
