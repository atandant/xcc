/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int a[2][3]; a[1][0] = 1; return *(*(a + 1)); }
