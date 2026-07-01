/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) { int a[2]; int *p; p=a; p[1]=10; return *(a+1); }
