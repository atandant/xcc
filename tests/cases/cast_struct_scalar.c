/* SPDX-License-Identifier: MIT */
/* expect: 5 */
/* Section 6.3: scalar to struct cast (D16) stores scalar bits */
struct S { int a; int b; };

int main(void) {
    struct S s;

    s = (struct S)5;
    return s.a;
}
