/* SPDX-License-Identifier: MIT */
/* expect-error: excess elements in array initializer */
int main(void) {
    int a[2] = {1, 2, 3};
    return a[0];
}
