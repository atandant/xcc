/* SPDX-License-Identifier: MIT */
/* expect: 0 */
struct Pair { int left; long right; };
int main(void) { static struct Pair pair; return pair.left != 0 || pair.right != 0; }
