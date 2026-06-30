/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int *id(int *p) { return p; }
int main(void) { int x; int *p; x = 13; p = id(&x); return *p; }
