/* SPDX-License-Identifier: MIT */
/* expect-error: array size is not an integer constant expression */
int main(void) {
    int n;
    int a[n];
    return 0;
}
