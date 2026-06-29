/* SPDX-License-Identifier: MIT */
/* expect-error: called object 'f' is not a function */
int main(void) {
    int f = 3;
    return f();
}
