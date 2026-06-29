/* SPDX-License-Identifier: MIT */
/* expect: 23 */
int pack(int a, int b) {
    return a * 10 + b;
}

int main(void) {
    int x = 2;
    return pack(x, x = 3);
}
