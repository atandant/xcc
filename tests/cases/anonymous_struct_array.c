/* SPDX-License-Identifier: MIT */
/* expect: 6 */

typedef struct { int value; } Item;

int main(void)
{
    Item items[3] = { { 1 }, { 2 }, { 3 } };
    return items[0].value + items[1].value + items[2].value;
}
