/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int f(void *v) { int *p; p = v; return *p; }
int main(void) { int x; x = 3; return f(&x); }
