/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) {
    int i; int s;
    i = 0; s = 0;
    while (i < 5) { s = s + i; i = i + 1; }
    return s;
}
