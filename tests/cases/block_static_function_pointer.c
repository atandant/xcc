/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int add(int value) { return value + 3; }
int main(void) { static int (*operation)(int) = add; return operation(4) - 7; }
