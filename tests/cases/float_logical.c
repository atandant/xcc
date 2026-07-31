/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) { double z = 0.0; double x = 4.0; return (!z) + (z || x) + (x && 2.0); }
