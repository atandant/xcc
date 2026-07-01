/* SPDX-License-Identifier: MIT */
/* expect-warning: implicit declaration of function 'foo' */
int main(void) { return foo(); }
int foo(void) { return 0; }
