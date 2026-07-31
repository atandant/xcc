/* SPDX-License-Identifier: MIT */
/* expect: 7 */
double add(double x, float y) { return x + y; }
int main(void) { double (*fn)(double, float) = add; return (int)fn(2.5, 5.0f); }
