/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of 'x' */
static int x = 1;
static int x = 2;
int main(void) { return x; }
