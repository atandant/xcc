/* SPDX-License-Identifier: MIT */
/* expect: 21 */
int add(int a, int b) {
    return a + b;
}

int mul(int a, int b) {
    return a * b;
}

int main(void) {
    return add(mul(2, 5), add(4, 7));
}
