/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'f' */
int f(void);
void f(void);

int main(void) {
    return 0;
}
