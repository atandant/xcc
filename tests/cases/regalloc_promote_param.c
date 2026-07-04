/* SPDX-License-Identifier: MIT */
int use(int n) { return n + 1; }
/* expect: 6 */
int main(void) {
    int n;
    n = 5;
    return use(n);
}
