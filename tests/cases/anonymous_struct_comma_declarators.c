/* SPDX-License-Identifier: MIT */
/* expect: 13 */

typedef struct { int value; } Item;

int main(void)
{
    Item item, *pointer = &item;
    pointer->value = 13;
    return item.value;
}
