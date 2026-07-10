/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int mul2(int x) { return x * 2; }
int main(void) {
    int (*fp)(int) = mul2;
    return (*fp)(5);
}
