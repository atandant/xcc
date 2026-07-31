/* SPDX-License-Identifier: MIT */
/* expect-error: macro name must be an identifier */
#undef
int main(void) { return 0; }
