/* SPDX-License-Identifier: MIT */
/* expect: 102 */
int main(void) {
    char text[3] = "abc";
    return sizeof(text) + text[2];
}
