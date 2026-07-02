/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) { int a[5]; int *p; p = a + 3; return p - a; }
