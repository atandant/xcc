/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int *bump(int *p) { return p+1; }
int main(void) { int a[2]; a[1]=5; return *bump(a); }
