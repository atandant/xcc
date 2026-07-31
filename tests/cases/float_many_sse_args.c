/* SPDX-License-Identifier: MIT */
/* expect: 36 */
double sum8(double a, double b, double c, double d, double e, double f, double g, double h) { return a+b+c+d+e+f+g+h; }
int main(void) { return (int)sum8(1,2,3,4,5,6,7,8); }
