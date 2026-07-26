/* SPDX-License-Identifier: MIT */
/* expect: 124 */
struct Name { char text[5]; int value; };
int main(void) {
    struct Name name = {"cat", 8};
    return name.text[2] + name.value;
}
