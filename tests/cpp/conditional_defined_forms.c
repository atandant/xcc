/* SPDX-License-Identifier: MIT */
/* expect: 11 */
#define FIRST
#define SECOND
#if defined FIRST && defined(SECOND) && !defined THIRD
int main(void) { return 11; }
#endif
