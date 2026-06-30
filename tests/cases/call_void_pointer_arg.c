/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int f(void *v) { return 0; }
int main(void) { int x; int *p; p = &x; return f(p); }
