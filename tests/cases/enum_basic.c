/* SPDX-License-Identifier: MIT */
/* expect: 2 */
/* Enum: implicit enumerator values start at 0 and increment. */
enum Color { RED, GREEN, BLUE };

int main(void) {
    return BLUE;   /* 0,1,2 -> 2 */
}
