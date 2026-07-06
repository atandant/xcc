/* SPDX-License-Identifier: MIT */
/* expect-error: invalid application of sizeof to incomplete type */
int main(void) {
    return sizeof(int[]);
}
