/* SPDX-License-Identifier: MIT */
/* expect: 9 */
double a[4] = { 1.0, 2.5, 0, 5.5 };
int main(void) { return (int)(a[0] + a[1] + a[2] + a[3]); }
