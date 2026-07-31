/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate macro parameter 'x' */
#define BAD(x, x) x
int main(void) { return 0; }
