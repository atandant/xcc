/* SPDX-License-Identifier: MIT */
/* expect: 6 */
#if 1
# if 0
int main(void) { return 1; }
# else
int main(void) { return 6; }
# endif
#else
int main(void) { return 2; }
#endif
