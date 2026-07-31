/* SPDX-License-Identifier: MIT */
/* expect: 10 */
double gd = (1.5 + 3) * 2.0;
float gf = 1.25f;
int main(void) { return (int)(gd + gf); }
