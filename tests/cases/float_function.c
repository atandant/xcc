/* SPDX-License-Identifier: MIT */
/* expect: 9 */
double add(double a, double b) { return a + b; }
float half(float x) { return x / 2.0f; }
int main(void) { return (int)add(half(7.0f), 6.0); }
