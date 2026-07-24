/* SPDX-License-Identifier: MIT */
/* expect: 42 */
struct Pair { int left; int right; };
struct Pair pair;
int main(void) { pair.left = 10; pair.right = 32; return pair.left + pair.right; }
