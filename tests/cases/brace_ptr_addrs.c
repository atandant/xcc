/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int a;
    int b;
    int *p[2] = {&a, &b};
    return p[0] != 0;
}
