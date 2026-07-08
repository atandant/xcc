/* SPDX-License-Identifier: MIT */
/* expect-error: variable 's' has incomplete type 'struct S' */
/* Section 2.4: cannot define an object of incomplete struct type. */
struct S;

int main(void) {
    struct S s;
    return 0;
}
