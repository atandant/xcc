/* SPDX-License-Identifier: MIT */
/* expect: 40 */
/* Section 4.2: array of structs with per-element brace inits */
struct Point { int x; int y; };

int main(void) {
    struct Point a[2] = { { 10, 20 }, { 30, 40 } };
    return a[0].x + a[1].x;
}
