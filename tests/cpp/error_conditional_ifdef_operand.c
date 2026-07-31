/* SPDX-License-Identifier: MIT */
/* expect-error: ifdef requires an identifier */
#ifdef FIRST SECOND
int main(void) { return 0; }
#endif
