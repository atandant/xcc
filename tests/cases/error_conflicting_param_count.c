/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'f' */
int f(int x);
int f(int x, int y);

int main(void) {
    return 0;
}
