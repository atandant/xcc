/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) {
    int n = 5;
    int count = 0;
    while (n = n - 1)
        count = count + 1;
    return count;
}
