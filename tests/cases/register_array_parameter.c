/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int first(register int a[]) { return a[0]; }
int main(void) { int a[1] = {7}; return first(a); }
