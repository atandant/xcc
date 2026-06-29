/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) {
    int i = 1;
    int sum = 0;
    while (i < 6) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
