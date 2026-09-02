/* SPDX-License-Identifier: MIT */
/* expect-error: initializer for file-scope object 'pointer' is not constant */
int integer;
static int *pointer = (int *)integer;
int main(void) { return pointer != 0; }
