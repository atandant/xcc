/* SPDX-License-Identifier: MIT */
/* expect-error: pasting '/' and '*' does not give a valid preprocessing token */
#define BAD / ## *
int main(void) { return BAD; }
