/* SPDX-License-Identifier: MIT */
/* expect: 3 */
/* Enum: an enumerator initializer may reference an earlier enumerator. */
enum E { A = 1, B = A + 2 };

int main(void) {
    return B;   /* 1 + 2 = 3 */
}
