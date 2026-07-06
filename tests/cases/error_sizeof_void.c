/* SPDX-License-Identifier: MIT */
/* expect-error: invalid application of sizeof to void type */
int main(void) {
    return sizeof(void);
}
