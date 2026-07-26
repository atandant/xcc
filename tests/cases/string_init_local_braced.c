/* SPDX-License-Identifier: MIT */
/* expect: 103 */
int main(void) {
    char text[] = {"abc"};
    return sizeof(text) + text[2];
}
