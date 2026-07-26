/* SPDX-License-Identifier: MIT */
/* expect: 67 */
int main(void) {
    char text[] = "\x41" "BC";
    return text[2];
}
