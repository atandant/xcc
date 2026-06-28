/* SPDX-License-Identifier: MIT */
/* expect-error: redeclaration of 'a' */
int main(void) {
    int a;
    int a;
    return 0;
}
