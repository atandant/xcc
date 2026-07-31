/* SPDX-License-Identifier: MIT */
/* expect: 15 */
#if (0 && (1 / 0)) || (1 || (1 / 0))
int main(void) { return 15; }
#endif
