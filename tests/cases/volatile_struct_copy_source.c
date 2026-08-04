/* SPDX-License-Identifier: MIT */
/* expect: 24 */
struct Pair { int left; int right; };
int main(void)
{
    volatile struct Pair source;
    struct Pair target;
    source.left = 9;
    source.right = 15;
    target = source;
    return target.left + target.right;
}
