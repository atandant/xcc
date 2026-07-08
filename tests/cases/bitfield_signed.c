/* SPDX-License-Identifier: MIT */
/* expect: 254 */
/* Section 5.3: signed int bit-field sign extension (-2 in 3 bits) */
struct S { int x : 3; };

int main(void) {
    struct S s;
    s.x = -2;
    return (unsigned char)s.x;
}
