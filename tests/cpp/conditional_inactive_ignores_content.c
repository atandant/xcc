/* SPDX-License-Identifier: MIT */
/* expect: 7 */
#if 0
#unknown preprocessing tokens
this is not valid C
#define HIDDEN 1
#endif
int main(void) { return 7; }
