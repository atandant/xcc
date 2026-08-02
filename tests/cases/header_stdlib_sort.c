/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <stdlib.h>

static int cmp_long(const void *a, const void *b)
{
    const long *la = (const long *)a;
    const long *lb = (const long *)b;

    if (*la < *lb)
        return -1;
    if (*la > *lb)
        return 1;
    return 0;
}

int main(void)
{
    long values[3];
    long key;
    long *found;

    values[0] = 3;
    values[1] = 1;
    values[2] = 2;
    qsort(values, 3, sizeof(values[0]), cmp_long);
    if (values[0] != 1 || values[2] != 3)
        return 1;
    key = 2;
    found = (long *)bsearch(&key, values, 3, sizeof(values[0]), cmp_long);
    if (!found || *found != 2)
        return 2;
    return 0;
}
