/* SPDX-License-Identifier: MIT */
/* expect: 30 */
/* Section 4.1: full struct brace initialization */
struct Point { int x; int y; };

int main(void) {
    struct Point p = { 10, 20 };
    return p.x + p.y;
}
