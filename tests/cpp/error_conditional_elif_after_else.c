/* SPDX-License-Identifier: MIT */
/* expect-error: #elif after #else */
#if 0
#else
int main(void) { return 0; }
#elif 1
#endif
