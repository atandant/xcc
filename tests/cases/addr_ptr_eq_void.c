/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; void *v; p = &x; v = p; return p == v; }
