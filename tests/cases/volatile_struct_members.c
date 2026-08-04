/* SPDX-License-Identifier: MIT */
/* expect: 18 */
struct Pair { int left; int right; };
int main(void)
{
    volatile struct Pair pair;
    pair.left = 7;
    pair.right = 11;
    return pair.left + pair.right;
}
