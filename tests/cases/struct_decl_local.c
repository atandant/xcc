/* SPDX-License-Identifier: MIT */
/* expect: 8 */
/* Section 2.4: local struct object declaration; sizeof of a struct lvalue. */
struct Point { int x; int y; };

int main(void) {
    struct Point p;
    return sizeof(p);
}
