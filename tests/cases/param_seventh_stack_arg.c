/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int seventh(int a, int b, int c, int d, int e, int f, int g) {
    return g;
}

int main(void) {
    return seventh(1, 2, 3, 4, 5, 6, 7);
}
