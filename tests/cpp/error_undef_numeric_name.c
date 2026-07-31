/* SPDX-License-Identifier: MIT */
/* expect-error: macro name must be an identifier */
#undef 123
int main(void) { return 0; }
