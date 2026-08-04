/* SPDX-License-Identifier: MIT */
/* expect: 27 */
struct Pair { int left; int right; };
int main(void)
{
    struct Pair source;
    volatile struct Pair target;
    source.left = 13;
    source.right = 14;
    target = source;
    return target.left + target.right;
}
