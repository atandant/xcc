/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) { int a[2]; int *p; a[1]=6; p=a; return p[1]; }
