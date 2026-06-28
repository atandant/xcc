/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) {
    int a = 10;
    return (a == 10) + (a != 5) + (a >= 10);
}
