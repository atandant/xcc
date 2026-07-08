/* SPDX-License-Identifier: MIT */
/* expect: 42 */
/* Section 3.2: load scalar struct member via `.` */
struct Point { int x; int y; };

int main(void) {
    struct Point p;
    p.x = 10;
    p.y = 42;
    return p.y;
}
