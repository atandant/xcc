/* SPDX-License-Identifier: MIT */
/* expect-error: hex escape sequence has no digits */
int main(void) { return "\x"[0]; }
