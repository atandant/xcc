/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) { int a[2][3]; a[1][2] = 7; return (*(a + 1))[2]; }
