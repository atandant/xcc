/* SPDX-License-Identifier: MIT */
/* expect-error: 'defined' requires an identifier */
#if defined()
int main(void) { return 0; }
#endif
