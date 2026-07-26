/* SPDX-License-Identifier: MIT */
/* expect-error: octal escape sequence out of range */
int main(void) { return "\777"[0]; }
