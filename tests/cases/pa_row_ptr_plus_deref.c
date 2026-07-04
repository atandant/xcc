/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) { int a[2][3]; a[1][0] = 4; return *(*(a + 1)); }
