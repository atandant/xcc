/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) {
    int a;
    int b;
    b = (a = 5) + 2;
    return b;
}
