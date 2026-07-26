/* SPDX-License-Identifier: MIT */
/* expect-error: unknown escape sequence '\q' */
int main(void) { return "\q"[0]; }
