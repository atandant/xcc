/* SPDX-License-Identifier: MIT */
/* expect: 4 */
/* Enum: an enum object has integer type (int) -> sizeof == 4 on LP64. */
enum E { X, Y, Z };

int main(void) {
    enum E e;
    e = Y;
    return sizeof(e);
}
