/* SPDX-License-Identifier: MIT */
/* expect-error: integer constant out of range */
int main(void) { return 0x10000000000000000; }
