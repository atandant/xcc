/* SPDX-License-Identifier: MIT */
/* expect: 15 */
struct Item { const int *pointer; int value; };
static const struct Item item = {(const int *)0, 15};
int main(void) { return item.pointer == 0 ? item.value : 1; }
