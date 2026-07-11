/* SPDX-License-Identifier: MIT */
/* expect: 7 */
struct Item { int value; };

int main(void)
{
    struct Item outer;

    outer.value = 7;
    {
        struct Item { long different; } inner;
        inner.different = 9;
    }
    return outer.value;
}
