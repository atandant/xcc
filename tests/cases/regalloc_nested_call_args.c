/* SPDX-License-Identifier: MIT */
int add3(int a, int b, int c) { return a + b + c; }
/* expect: 24 */
int main(void) {
    int x; int y; int z;
    x = 3; y = 4; z = 5;
    return add3(x + 3, y + 4, z + 5);
}
