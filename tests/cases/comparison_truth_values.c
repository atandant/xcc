/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) {
    return (3 < 4) + (4 <= 4) + (5 > 4) + (5 >= 5) + (5 != 6) + (6 == 6);
}
