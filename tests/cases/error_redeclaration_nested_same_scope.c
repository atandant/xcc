/* SPDX-License-Identifier: MIT */
/* expect-error: redeclaration of 'x' */
int main(void) {
    {
        int x;
        int x;
    }
    return 0;
}
