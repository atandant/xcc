/* SPDX-License-Identifier: MIT */
/* expect: 7 */
/* Section 3.4: address-of member */
struct Point { int x; int y; };

int main(void) {
    struct Point p;
    int *q;
    p.x = 7;
    p.y = 0;
    q = &p.x;
    return *q;
}
