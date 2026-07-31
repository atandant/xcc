/* SPDX-License-Identifier: MIT */
/* expect: 21 */
#define ADD(a, b) ((a) + (b))
#if ADD(2, 3) == 5
int main(void) { return 21; }
#else
int main(void) { return 1; }
#endif
