/* SPDX-License-Identifier: MIT */
/* expect: 0 */
struct Pair { int left; int right; };
int main(void) { static struct Pair pair = { 7, 11 }; return pair.right - 11; }
