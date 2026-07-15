/* SPDX-License-Identifier: MIT */
/* expect: 21 */
int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}

int call6(int (*fn)(int, int, int, int, int, int),
          int a, int b, int c, int d, int e, int f) {
    return fn(a, b, c, d, e, f);
}

int main(void) {
    return call6(sum6, 1, 2, 3, 4, 5, 6);
}
