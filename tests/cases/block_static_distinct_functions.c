/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int f(void) { static int value; return ++value; }
int g(void) { static int value; return ++value; }
int main(void) { f(); return f() + g() - 3; }
