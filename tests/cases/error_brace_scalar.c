/* SPDX-License-Identifier: MIT */
/* expect-error: brace initializer cannot be used to initialize 'int' */
int main(void) {
    int x = {3};
    return x;
}
