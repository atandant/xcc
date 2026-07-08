/* SPDX-License-Identifier: MIT */
/* expect: 8 */
typedef int A;
typedef A B;

int main(void) {
    B x;
    x = 8;
    return x;
}
