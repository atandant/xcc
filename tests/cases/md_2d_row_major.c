/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) { int a[2][3]; int *p; a[1][2] = 5; p = &a[0][0]; return *(p + 5); }
