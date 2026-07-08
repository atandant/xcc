/* SPDX-License-Identifier: MIT */
/* expect: 99 */
/* Section 3.3: assign to struct member lvalue */
struct Point { int x; int y; };

int main(void) {
    struct Point p;
    p.x = 1;
    p.y = 2;
    p.y = 99;
    return p.y;
}
