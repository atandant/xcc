/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stddef.h>

struct Pair {
    int first;
    long second;
};

int main(void)
{
    int values[4];
    if (NULL != 0)
        return 1;
    if (sizeof(size_t) != sizeof(long))
        return 2;
    if (offsetof(struct Pair, second) != 8)
        return 3;
    if (sizeof(values) / sizeof(values[0]) != 4)
        return 4;
    return 0;
}
