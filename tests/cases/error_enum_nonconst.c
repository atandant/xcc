/* SPDX-License-Identifier: MIT */
/* expect-error: enumerator value is not an integer constant expression */
/* Enum: enumerator initializers must be integer constant expressions. */
int f(void) { return 3; }
enum E { A = f() };

int main(void) {
    return A;
}
