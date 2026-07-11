/* SPDX-License-Identifier: MIT */
/* expect: 7 */
struct Item { int value; };

int main(void)
{
    {
        struct Item { long different; } inner;
        inner.different = 9;
    }
    {
        struct Item outer;
        outer.value = 7;
        return outer.value;
    }
}
