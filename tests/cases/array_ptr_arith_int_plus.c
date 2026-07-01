/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) { int a[3]; int *p; a[1]=5; p=a; return *(1+p); }
