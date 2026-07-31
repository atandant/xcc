/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) { double a; double b; float f; a = b = 4.25; f = a; return (int)(a + b + f); }
