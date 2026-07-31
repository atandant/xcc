/* SPDX-License-Identifier: MIT */
/* expect-error: invalid storage class for function 'f' */
register int f(void);
int main(void) { return 0; }
