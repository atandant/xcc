/* SPDX-License-Identifier: MIT */
/* expect: 7 */
/* Section 4.1 / D12: partial struct init zero-fills tail members */
struct Point { int x; int y; };

int main(void) {
    struct Point p = { 7 };
    return p.x + p.y;
}
