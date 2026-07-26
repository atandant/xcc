/* SPDX-License-Identifier: MIT */
/* expect: 102 */
int main(void) {
    char text[] = "a\0b";
    return sizeof(text) + text[2];
}
