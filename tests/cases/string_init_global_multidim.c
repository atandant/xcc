/* SPDX-License-Identifier: MIT */
/* expect: 218 */
char text[2][4] = { "ab", "xy" };
int main(void) { return text[0][1] + text[1][0]; }
