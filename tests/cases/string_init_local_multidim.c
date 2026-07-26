/* SPDX-License-Identifier: MIT */
/* expect: 218 */
int main(void) {
    char text[2][4] = {"ab", "xy"};
    return text[0][1] + text[1][0];
}
