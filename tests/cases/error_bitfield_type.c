/* SPDX-License-Identifier: MIT */
/* expect-error: bit-field has invalid type */
struct S { char c : 3; };

int main(void) {
    return sizeof(struct S);
}
