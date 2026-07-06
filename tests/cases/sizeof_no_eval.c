/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int side(int *p) {
    *p = 1;
    return 0;
}

int main(void) {
    int g;
    g = 0;
    (void)(sizeof(side(&g)));
    return g;
}
