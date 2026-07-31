/* SPDX-License-Identifier: MIT */
/* expect: 13 */
#if UNKNOWN_IDENTIFIER
int main(void) { return 1; }
#else
int main(void) { return 13; }
#endif
