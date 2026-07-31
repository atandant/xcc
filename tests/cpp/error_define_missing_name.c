/* SPDX-License-Identifier: MIT */
/* expect-error: macro name must be an identifier */
#define
int main(void) { return 0; }
