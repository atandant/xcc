/* SPDX-License-Identifier: MIT */
/* expect-error: returning struct type 'struct S' by value is not supported */
struct S { int x; };

struct S f(void) {
    struct S s;
    s.x = 1;
    return s;
}

int main(void) {
    return 0;
}
