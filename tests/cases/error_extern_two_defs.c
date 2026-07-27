/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of 'x' */
extern int x;
int x = 1;
int x = 2;
int main(void) { return x; }
