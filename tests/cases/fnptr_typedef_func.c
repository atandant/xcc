/* SPDX-License-Identifier: MIT */
/* expect: 9 */
typedef int F(int);
int sq(int x) { return x * x; }
int main(void) {
    F *p = sq;
    return p(3);
}
