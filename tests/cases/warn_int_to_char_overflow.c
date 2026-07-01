/* SPDX-License-Identifier: MIT */
/* expect-warning: overflow in conversion from 'int' to 'char' */
int main(void) {
    char c;
    c = 300;
    return 0;
}
