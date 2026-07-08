/* SPDX-License-Identifier: MIT */
/* expect: 1 */
/* Section 4.3: struct assign copies bits; mutating source does not affect copy */
struct Point { int x; int y; };

int main(void) {
    struct Point a;
    struct Point b;
    a.x = 1;
    a.y = 2;
    b = a;
    a.x = 99;
    return b.x;
}
