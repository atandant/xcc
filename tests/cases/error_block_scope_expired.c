/* SPDX-License-Identifier: MIT */
/* expect-error: use of undeclared identifier 'x' */
int main(void) {
    {
        int x = 1;
    }
    return x;
}
