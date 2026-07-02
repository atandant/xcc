/* SPDX-License-Identifier: MIT */
/* expect: 30 */
int sum(int a[2][3]) { return a[0][0] + a[0][1] + a[0][2]; }
int main(void) { int a[2][3]; a[0][0] = 5; a[0][1] = 10; a[0][2] = 15; return sum(a); }
