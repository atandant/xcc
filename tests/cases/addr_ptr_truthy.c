/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; p = &x; if (p) return 1; return 0; }
