/* SPDX-License-Identifier: MIT */
/* expect-error: passing struct type 'struct S' by value is not supported */
struct S { int x; };

void f(struct S s) {
    (void)s;
}

int main(void) {
    struct S s;
    f(s);
    return 0;
}
