/* SPDX-License-Identifier: MIT */
/* expect-error: passing 'int' to parameter of type 'int *' in call to 'f' */
int f(int *p) { return 0; }
int main(void) { int x; return f(x); }
