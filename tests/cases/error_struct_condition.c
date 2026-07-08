/* SPDX-License-Identifier: MIT */
/* expect-error: condition has non-scalar type 'struct S' */
struct S { int x; };

int main(void) {
    struct S s;

    if (s)
        return 1;
    return 0;
}
