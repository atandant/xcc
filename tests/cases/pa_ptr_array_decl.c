/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int x;
    int *p[2];
    p[0] = &x;
    return p[0] != 0;
}
