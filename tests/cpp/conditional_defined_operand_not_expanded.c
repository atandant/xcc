/* SPDX-License-Identifier: MIT */
/* expect: 12 */
#define ALIAS TARGET
#if defined ALIAS && !defined TARGET
int main(void) { return 12; }
#endif
