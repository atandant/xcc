/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate #else */
#if 0
#else
int main(void) { return 0; }
#else
#endif
