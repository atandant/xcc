/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) {
    int a[sizeof(int)];
    a[0] = 4;
    return a[0];
}
