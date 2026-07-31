/* SPDX-License-Identifier: MIT */
/* expect: 10 */
#define FIRST SECOND
#define SECOND 2
#if FIRST + 3 == 5
int main(void) { return 10; }
#else
int main(void) { return 1; }
#endif
