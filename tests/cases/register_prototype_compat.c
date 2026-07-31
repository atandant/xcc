/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int add(register int, int);
int add(int x, register int y) { return x + y; }
int main(void) { return add(3, 5); }
