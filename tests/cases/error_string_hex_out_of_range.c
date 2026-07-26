/* SPDX-License-Identifier: MIT */
/* expect-error: hex escape sequence out of range */
int main(void) { return "\x100"[0]; }
