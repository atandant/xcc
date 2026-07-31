/* SPDX-License-Identifier: MIT */
/* expect: 9 */
struct Pair { int x; int y; };
int main(void) { register struct Pair p = {4, 5}; return p.x + p.y; }
