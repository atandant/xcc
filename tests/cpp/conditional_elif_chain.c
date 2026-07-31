/* SPDX-License-Identifier: MIT */
/* expect: 4 */
#if 0
int main(void) { return 1; }
#elif 0
int main(void) { return 2; }
#elif 1
int main(void) { return 4; }
#else
int main(void) { return 8; }
#endif
