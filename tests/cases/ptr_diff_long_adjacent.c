/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int main(void) { int a[5]; int *p; int *q; p = a + 1; q = a + 3; return q - p; }
