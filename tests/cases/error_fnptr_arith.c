/* SPDX-License-Identifier: MIT */
/* expect-error: invalid operands to arithmetic operator */
int f(int);
int main(void) {
    int (*fp)(int) = f;
    return (int)(fp + 1);
}
