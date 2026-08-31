/* SPDX-License-Identifier: MIT */
/* expect: 15 */

typedef struct { int value; } Item;

Item make_item(int value)
{
    Item item;
    item.value = value;
    return item;
}

int read_item(Item item)
{
    return item.value;
}

int main(void)
{
    return read_item(make_item(15));
}
