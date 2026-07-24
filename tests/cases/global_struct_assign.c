/* SPDX-License-Identifier: MIT */
/* expect: 11 */
struct Pair { int x; int y; };
struct Pair left;
struct Pair right;
int main(void) { left.x = 5; left.y = 6; right = left; return right.x + right.y; }
