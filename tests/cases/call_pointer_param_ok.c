/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int f(int *p) { return 0; }
int main(void) { int *p; return f(p); }
