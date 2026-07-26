/* SPDX-License-Identifier: MIT */
/* expect: 1 */
struct Pair { int x; int y; };
struct Pair pair = { 1 };

int main(void) { return pair.x + pair.y; }
