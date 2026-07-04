/* SPDX-License-Identifier: MIT */
int id(int x) { return x; }
/* expect: 10 */
int main(void) {
    int a; int b;
    a = 5;
    b = id(a);
    return a + b;
}
