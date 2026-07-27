/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting linkage for 'x' */
int x;
static int x;
int main(void) { return x; }
