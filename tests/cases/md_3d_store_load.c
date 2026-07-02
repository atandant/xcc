/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) { int a[2][2][2]; a[1][1][1] = 9; return a[1][1][1]; }
