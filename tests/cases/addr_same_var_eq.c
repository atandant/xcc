/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; int *q; p = &x; q = &x; return p == q; }
