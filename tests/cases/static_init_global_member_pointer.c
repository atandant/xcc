/* SPDX-License-Identifier: MIT */
/* expect: 41 */
struct Box { int pad; int value; };
struct Box box = { 1, 41 };
int *selected = &box.value;
int main(void) { return *selected; }
