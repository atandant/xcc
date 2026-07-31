/* SPDX-License-Identifier: MIT */
/* expect-error: macro parameter must be an identifier */
#define BAD(x,) x
int main(void) { return 0; }
