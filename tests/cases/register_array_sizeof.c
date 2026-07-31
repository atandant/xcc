/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    register int a[3];
    return sizeof(a) == 3 * sizeof(int);
}
