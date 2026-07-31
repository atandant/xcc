/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { double z = 0.0; double n = z / z; if (n == n) return 2; if (n < 1.0) return 3; if (n <= 1.0) return 4; if (n > 1.0) return 5; if (n >= 1.0) return 6; return n != n; }
