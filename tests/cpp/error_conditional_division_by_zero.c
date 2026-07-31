/* SPDX-License-Identifier: MIT */
/* expect-error: division by zero in #if expression */
#if 1 / 0
int main(void) { return 0; }
#endif
