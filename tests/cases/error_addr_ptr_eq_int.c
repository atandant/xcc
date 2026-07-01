/* SPDX-License-Identifier: MIT */
/* expect-error: comparison between pointer type 'int *' and integer type 'int' */
int main(void) { int *p; return p == 3; }
