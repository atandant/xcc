/* SPDX-License-Identifier: MIT */
/* expect-error: initializer for block-scope static object 'value' is not constant */
int make(void) { return 3; }
int main(void) { static int value = make(); return value; }
