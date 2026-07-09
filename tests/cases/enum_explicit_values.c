/* SPDX-License-Identifier: MIT */
/* expect: 11 */
/* Enum: explicit value resets the counter; the next enumerator continues. */
enum E { A = 5, B = 10, C };

int main(void) {
    return C;   /* 5, 10, 11 */
}
