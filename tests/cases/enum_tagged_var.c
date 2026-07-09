/* SPDX-License-Identifier: MIT */
/* expect: 1 */
/* Enum: declare a variable of enum type, assign an enumerator, read it back. */
enum Color { RED, GREEN, BLUE };

int main(void) {
    enum Color c;
    c = GREEN;
    return c;   /* 1 */
}
