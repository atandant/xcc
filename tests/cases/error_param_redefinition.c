/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of parameter 'x' */
int f(int x, int x) {
    return x;
}

int main(void) {
    return f(1, 2);
}
