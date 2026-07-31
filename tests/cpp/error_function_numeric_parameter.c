/* SPDX-License-Identifier: MIT */
/* expect-error: macro parameter must be an identifier */
#define BAD(1) 1
int main(void) { return 0; }
