/* SPDX-License-Identifier: MIT */
/* expect: 12 */

typedef struct { int value; } Item;

int main(void)
{
    Item item;
    Item *pointer = &item;
    pointer->value = 12;
    return item.value;
}
