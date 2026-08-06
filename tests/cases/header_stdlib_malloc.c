/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdlib.h>

int main(void)
{
    void *p;
    void *q;

    p = malloc(32);
    if (!p)
        return 1;
    q = realloc(p, 64);
    if (!q)
        return 2;
    free(q);
    p = calloc(4, 8);
    if (!p)
        return 3;
    free(p);
    return 0;
}
