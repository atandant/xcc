/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int value = 4;
int get_global(void) { return value; }
int main(void) { int value; value = 9; return value + get_global(); }
