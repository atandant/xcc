/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) {
    int n = 4;
    int count = 0;
    for (; n = n - 1;)
        count = count + 1;
    return count;
}
