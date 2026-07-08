/* SPDX-License-Identifier: MIT */
/* expect-error: invalid operands to comparison */
struct S { int x; };

int main(void) {
    struct S a;
    struct S b;

    return a == b;
}
