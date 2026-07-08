/* SPDX-License-Identifier: MIT */
/* expect: 8 */
/* Section 2.5: forward declaration then completion of the same tag. */
struct S;
struct S { int x; int y; };

int main(void) {
    struct S s;
    return sizeof(s);
}
