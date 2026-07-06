/* SPDX-License-Identifier: MIT */
/* expect-error: too many braces around scalar initializer */
int main(void) {
    int x = {{3}};
    return x;
}
