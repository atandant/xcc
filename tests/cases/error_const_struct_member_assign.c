/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to const-qualified object */
struct Item { int value; };

int main(void)
{
    const struct Item item = { 1 };
    item.value = 2;
    return item.value;
}
