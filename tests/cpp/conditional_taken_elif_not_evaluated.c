/* SPDX-License-Identifier: MIT */
/* expect: 5 */
#if 1
int main(void) { return 5; }
#elif 1 / 0
int main(void) { return 6; }
#endif
