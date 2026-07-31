/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to const-qualified object */
struct Item { const int fixed; int mutable; };

int main(void)
{
    struct Item first = { 1, 2 };
    struct Item second = { 3, 4 };
    first = second;
    return first.mutable;
}
