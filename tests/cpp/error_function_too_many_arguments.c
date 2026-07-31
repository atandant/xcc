/* SPDX-License-Identifier: MIT */
/* expect-error: macro 'ID' requires 1 arguments, but 2 given */
#define ID(x) x
int main(void) { return ID(1, 2); }
