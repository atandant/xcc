/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'x' */
extern int x;
long x;
int main(void) { return 0; }
