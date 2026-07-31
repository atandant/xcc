/* SPDX-License-Identifier: MIT */
/* expect: 20 */
double twice(double x) { return x * 2.0; }
int main(void) { double a = 3.0; double b = 4.0; double c = 5.0; return (int)(a + twice(b) + c + twice(2.0)); }
