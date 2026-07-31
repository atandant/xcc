/* SPDX-License-Identifier: MIT */
/* expect: 20 */
#define CAT(a, b) a ## b
int main(void) { int value CAT(, ) = 20; return value; }
