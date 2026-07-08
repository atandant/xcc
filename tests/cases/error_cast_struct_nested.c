/* SPDX-License-Identifier: MIT */
/* expect-error: cast to non-scalar type 'struct Outer' */
struct Inner { int x; };
struct Outer { struct Inner i; };

int main(void) {
    return (struct Outer)42;
}
