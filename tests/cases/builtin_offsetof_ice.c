/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stddef.h>

struct Inner {
    char tag;
    long value;
};

struct Outer {
    char prefix;
    struct Inner inner;
};

char padding[offsetof(struct Outer, inner.value)];

int main(void)
{
    return sizeof(padding) != 16 ||
           sizeof(offsetof(struct Outer, inner.value)) != sizeof(size_t);
}
