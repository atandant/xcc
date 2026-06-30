/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; p = &x; if (p == 0) return 0; if (0 == p) return 0; return p != 0; }
