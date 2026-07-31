/* SPDX-License-Identifier: MIT */
/* expect: 16 */
double power2(double x, int n) { if (!n) return x; return power2(x * 2.0, n - 1); }
int main(void) { return (int)power2(2.0, 3); }
