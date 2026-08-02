/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <stdlib.h>

int main(void)
{
    int a;
    int b;

    srand(1);
    a = rand();
    srand(1);
    b = rand();
    if (a != b)
        return 1;
    if (a == 0 && b == 0)
        return 2;
    return 0;
}
