/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int g();
int main(void) { int *p; return g(p); }
int g(int *p) { return 0; }
