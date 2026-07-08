/* SPDX-License-Identifier: MIT */
/* expect: 17 */
/* Section 3.3: load member via `->` (desugared to (*p).member) */
struct Point { int x; int y; };

int main(void) {
    struct Point s;
    struct Point *p;
    s.x = 17;
    s.y = 3;
    p = &s;
    return p->x;
}
