/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int value = 1;
    // The assignment remains in this comment after splicing. \
    value = 99;
    return value;
}
