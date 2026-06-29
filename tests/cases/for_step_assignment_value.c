/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) {
    int i = 0;
    int sum = 0;
    for (i = 1; i <= 4; i = i + 1)
        sum = sum + i;
    return sum;
}
