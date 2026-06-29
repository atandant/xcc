/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) {
    return later(2, 3);
}

int later(int a, int b) {
    return a * b;
}
