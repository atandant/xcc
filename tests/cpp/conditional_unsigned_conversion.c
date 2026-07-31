/* SPDX-License-Identifier: MIT */
/* expect: 18 */
#if (-1 < 1U) || !(0xffffffff > -1)
int main(void) { return 1; }
#else
int main(void) { return 18; }
#endif
