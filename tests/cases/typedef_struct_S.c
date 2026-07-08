/* SPDX-License-Identifier: MIT */
/* expect: 8 */
/* Section 2.6: `typedef struct S S;` before the tag body (A11 + struct). */
typedef struct S S;
struct S { int x; int y; };

int main(void) {
    S s;
    return sizeof(s);
}
