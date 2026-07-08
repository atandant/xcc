/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of bit-field */
struct S { int x : 3; };

int main(void) {
    struct S s;
    int *p;
    p = &s.x;
    return 0;
}
