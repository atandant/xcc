/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int *id(int *p) { return p; }
int main(void) { int x; int *p; p = id(&x); *p = 11; return x; }
