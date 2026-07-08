/* SPDX-License-Identifier: MIT */
/* expect: 55 */
/* Section 3.3: store through `->` */
struct Point { int x; int y; };

int main(void) {
    struct Point s;
    struct Point *p;
    p = &s;
    p->x = 55;
    p->y = 0;
    return s.x;
}
