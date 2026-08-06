/* SPDX-License-Identifier: MIT */
/* expect-error: too few arguments to function 'f' */
int f(int, ...);
int main(void) { return f(); }
