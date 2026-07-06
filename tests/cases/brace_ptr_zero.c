/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int *p[2] = {0, 0};
    return p[0] == 0;
}
