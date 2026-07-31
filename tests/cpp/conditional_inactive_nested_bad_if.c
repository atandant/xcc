/* SPDX-License-Identifier: MIT */
/* expect: 8 */
#if 0
# if 1 +
invalid C
# endif
#endif
int main(void) { return 8; }
