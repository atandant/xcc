/* SPDX-License-Identifier: MIT */
/* expect-error: invalid application of sizeof to function type */
int f(void);

int main(void) {
    return sizeof(f);
}
