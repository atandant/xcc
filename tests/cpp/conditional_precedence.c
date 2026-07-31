/* SPDX-License-Identifier: MIT */
/* expect: 14 */
#if 1 + 2 * 3 == 7 && 8 >> 2 == 2 && 9 % 4 == 1 && 8 / 2 == 4 && 5 - 3 == 2
int main(void) { return 14; }
#endif
