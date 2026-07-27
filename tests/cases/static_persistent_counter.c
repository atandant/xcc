/* SPDX-License-Identifier: MIT */
/* expect: 0 */
static int s = 5;
int bump(void) { s++; return s; }
int main(void) { bump(); bump(); return bump() - 8; }
