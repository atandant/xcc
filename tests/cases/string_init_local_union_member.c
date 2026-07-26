/* SPDX-License-Identifier: MIT */
/* expect: 90 */
union Value { char text[4]; int number; };
int main(void) {
    union Value value = {"AZ"};
    return value.text[1];
}
