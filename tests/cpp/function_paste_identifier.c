/* SPDX-License-Identifier: MIT */
/* expect: 15 */
#define CAT(a, b) a ## b
int joined = 15;
int main(void) { return CAT(join, ed); }
