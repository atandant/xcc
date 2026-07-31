/* SPDX-License-Identifier: MIT */
/* expect-error: invalid storage class for block-scope function 'f' */
int main(void) { register int f(void); return 0; }
