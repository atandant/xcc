/* SPDX-License-Identifier: MIT */
/* expect-error: invalid storage class for file-scope object 'x' */
register int x;
int main(void) { return 0; }
