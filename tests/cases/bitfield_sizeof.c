/* SPDX-License-Identifier: MIT */
/* expect: 8 */
/* Section 5.2: bitfield allocation units affect struct size */
struct B { int a : 3; int b : 5; int : 0; int d : 4; };

int main(void) {
    return sizeof(struct B);
}
