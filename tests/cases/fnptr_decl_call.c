/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int add(int x) { return x + 1; }
int main(void) {
    int (*fp)(int);
    fp = add;
    return fp(41);
}
