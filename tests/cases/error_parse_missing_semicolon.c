/* SPDX-License-Identifier: MIT */
/* expect-error: syntax error, unexpected RETURN, expecting ';' */
int main(void) {
    int x = 1
    return x;
}
