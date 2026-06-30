/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) { int x; int *p; void *v; p = &x; v = p; p = v; *p = 9; return x; }
