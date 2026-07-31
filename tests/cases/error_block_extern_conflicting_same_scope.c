/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'value' */
int main(void) { extern int value; extern long value; return 0; }
