/* SPDX-License-Identifier: MIT */
/* expect-error: passing 'int *' to parameter of type 'int' in call to 'f' */
int f(int x) { return x; }
int main(void) { int *p; return f(p); }
