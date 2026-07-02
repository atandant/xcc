/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) { char m[2][3]; m[0][0] = 1; m[0][1] = 2; m[0][2] = 3; return m[0][0] + m[0][1] + m[0][2] + m[1][0] * 0; }
