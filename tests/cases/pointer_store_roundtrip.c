/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int main(void) { int x; int *p; p = &x; *p = 42; return *p; }
