/* SPDX-License-Identifier: MIT */
/* expect-error: brace initialization is not supported for 'short' arrays yet */
int main(void) {
    short a[2] = {1, 2};
    return a[0];
}
