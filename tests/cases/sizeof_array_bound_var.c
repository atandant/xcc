/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) {
    int x;
    int a[sizeof(x) * 3];
    x = 0;
    a[11] = 12;
    return a[11];
}
