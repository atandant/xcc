/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    return (int)((2147483647L + 1L) - 2147483648L);
}
