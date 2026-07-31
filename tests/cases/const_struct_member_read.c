/* SPDX-License-Identifier: MIT */
/* expect: 22 */
struct Pair { int left; int right; };
const static struct Pair global_pair = { 8, 11 };

int main(void)
{
    const struct Pair local_pair = { 1, 2 };
    const int *left = &local_pair.left;
    return global_pair.left + global_pair.right + *left + local_pair.right;
}
