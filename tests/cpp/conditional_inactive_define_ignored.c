/* SPDX-License-Identifier: MIT */
/* expect: 9 */
#if 0
#define VALUE 1
#endif
#ifndef VALUE
int main(void) { return 9; }
#endif
