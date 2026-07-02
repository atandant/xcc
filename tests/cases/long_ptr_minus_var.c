/* SPDX-License-Identifier: MIT */
/* expect: 33 */
int main(void) { int a[6]; int *p; long i; p = a + 5; i = 2L; a[3] = 33; return *(p - i); }
