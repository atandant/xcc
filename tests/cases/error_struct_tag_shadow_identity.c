/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'struct Item *' to 'struct Item *' */
struct Item { int value; };

int main(void)
{
    struct Item outer;
    struct Item *outer_ptr;

    outer_ptr = &outer;
    {
        struct Item { long different; } inner;
        struct Item *inner_ptr;
        inner_ptr = outer_ptr;
    }
    return 0;
}
