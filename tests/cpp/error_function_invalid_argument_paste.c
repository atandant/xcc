/* SPDX-License-Identifier: MIT */
/* expect-error: pasting '+' and '*' does not give a valid preprocessing token */
#define CAT(a, b) a ## b
int main(void) { return CAT(+, *); }
