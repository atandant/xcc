/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { int x; int y; int *p; int *q; p = &x; q = &y; return p == q; }
