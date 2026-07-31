/* SPDX-License-Identifier: MIT */
/* expect-error: redeclaration of 'value' */
int main(void) { int value; extern int value; return 0; }
