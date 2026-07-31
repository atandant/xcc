/* SPDX-License-Identifier: MIT */
/* expect: 16 */
#if (1 ? 4 : (1 / 0)) == 4 && (0 ? (1 / 0) : 5) == 5
int main(void) { return 16; }
#endif
