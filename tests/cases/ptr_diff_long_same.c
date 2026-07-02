/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { int a[4]; int *p; p = a + 2; return p - p; }
