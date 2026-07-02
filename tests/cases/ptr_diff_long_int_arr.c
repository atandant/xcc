/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) { int a[8]; int *p; p = a + 5; return p - a; }
