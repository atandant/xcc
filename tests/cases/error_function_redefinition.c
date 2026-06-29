/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of 'f' */
int f(void) {
    return 1;
}

int f(void) {
    return 2;
}

int main(void) {
    return f();
}
