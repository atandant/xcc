/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; p = &x; while (p) return 1; return 0; }
