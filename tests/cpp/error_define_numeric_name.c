/* SPDX-License-Identifier: MIT */
/* expect-error: macro name must be an identifier */
#define 123 VALUE
int main(void) { return 0; }
