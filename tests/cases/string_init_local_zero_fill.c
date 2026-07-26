/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    char text[8] = "abc";
    return text[3] + text[7];
}
