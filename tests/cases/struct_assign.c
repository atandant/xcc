/* SPDX-License-Identifier: MIT */
/* expect: 42 */
/* Section 4.3: struct assignment via MEMCPY */
struct Point { int x; int y; };

int main(void) {
    struct Point a;
    struct Point b;
    a.x = 42;
    a.y = 0;
    b = a;
    return b.x;
}
