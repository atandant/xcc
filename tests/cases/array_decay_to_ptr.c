/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) { int a[2]; int *p; p=a; p[1]=7; return a[1]; }
