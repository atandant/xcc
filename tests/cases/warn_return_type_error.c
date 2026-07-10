/* SPDX-License-Identifier: MIT */
/* expect-error: non-void function 'f' should return a value */
/* xcc-args: -Werror=return-type */
int f(void) { return; }
int main(void) { return f(); }
