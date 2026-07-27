/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting linkage for 'x' */
extern int x = 1;
static int x;
int main(void) { return x; }
