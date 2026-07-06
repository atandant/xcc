/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) {
    char a[sizeof(int) * 2];
    a[7] = 8;
    return a[7];
}
