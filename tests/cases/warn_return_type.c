/* SPDX-License-Identifier: MIT */
/* expect-warning: non-void function 'f' should return a value */
int f(void) { return; }
int main(void) { return f(); }
