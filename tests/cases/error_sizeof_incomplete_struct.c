/* SPDX-License-Identifier: MIT */
/* expect-error: invalid application of sizeof to incomplete type */
struct S;

int main(void) {
    return sizeof(struct S);
}
