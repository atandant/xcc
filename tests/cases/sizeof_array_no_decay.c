/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) {
    int a[10];
    return sizeof(a) / sizeof(int);
}
