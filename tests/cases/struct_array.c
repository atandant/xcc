/* SPDX-License-Identifier: MIT */
/* expect: 80 */
/* Section 2.4: array of structs; layout is count * sizeof(element). */
struct Point { int x; int y; };

int main(void) {
    struct Point a[10];
    return sizeof(a);
}
