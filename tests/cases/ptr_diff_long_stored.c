/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) { int a[6]; int *p; long d; p = a + 4; d = p - a; return d; }
