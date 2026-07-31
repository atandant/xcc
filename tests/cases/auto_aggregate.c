/* SPDX-License-Identifier: MIT */
/* expect: 9 */
struct Pair { int x; int y; };
int main(void) {
    auto int a[2] = {2, 3};
    auto struct Pair p = {1, 3};
    return a[0] + a[1] + p.x + p.y;
}
