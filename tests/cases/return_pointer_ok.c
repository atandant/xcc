/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int *f(void) { int *p; return p; }
int main(void) { f(); return 0; }
