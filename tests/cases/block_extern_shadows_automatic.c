/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int value = 12;
int main(void) { int value = 3; { extern int value; if (value != 12) return 1; } return value - 3; }
