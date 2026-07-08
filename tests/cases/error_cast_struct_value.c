/* SPDX-License-Identifier: MIT */
/* expect-error: operand of cast has non-scalar type 'struct S' */
struct S { int x; };

int main(void) {
    struct S s;
    return (int)s;
}
