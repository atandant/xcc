/* SPDX-License-Identifier: MIT */
/* expect-error: pasting '+' and '*' does not give a valid preprocessing token */
#define BAD + ## *
int main(void) { return 1 BAD 2; }
