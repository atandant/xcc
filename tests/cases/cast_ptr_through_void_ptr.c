/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) { int x; int *p; void *q; x = 7; p = &x; q = (void *)p; return *(int *)q; }
