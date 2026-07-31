/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) { int yes = 1; float f = yes ? 4.5f : 1; double d = 0 ? 2.0f : 5.0; return (int)(f + d); }
