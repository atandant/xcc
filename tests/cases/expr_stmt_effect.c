/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) {
    int a = 2;
    a + 100;
    a = a + 5;
    return a;
}
