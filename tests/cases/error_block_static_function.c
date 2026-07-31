/* SPDX-License-Identifier: MIT */
/* expect-error: invalid storage class for block-scope function 'helper' */
int main(void) { static int helper(void); return 0; }
