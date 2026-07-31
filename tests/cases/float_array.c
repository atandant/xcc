/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int main(void) { float a[3] = { 1, 1.5, 2.5f }; double b[2] = { 3, 4.5f }; a[2] += a[0]; return (int)(a[0] + a[1] + a[2] + b[0] + b[1]); }
