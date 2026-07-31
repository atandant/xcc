/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) { float z = 0.0f; double n = -2.0; if (z) return 1; if (n) return 7; return 2; }
