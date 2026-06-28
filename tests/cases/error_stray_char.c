/* SPDX-License-Identifier: MIT */
/* expect-error: stray '@' in program */
int main(void) {
    return @;
}
