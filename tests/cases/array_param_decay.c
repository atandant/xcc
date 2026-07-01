/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int set0(int x[]) { x[0]=11; return 0; }
int main(void) { int a[1]; a[0]=0; set0(a); return a[0]; }
