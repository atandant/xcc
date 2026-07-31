/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int first(void) { extern int value; return value; }
int second(void) { extern int value; return value + 1; }
int value = 20;
int main(void) { return first() + second() - 41; }
