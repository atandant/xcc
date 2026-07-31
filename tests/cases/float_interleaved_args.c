/* SPDX-License-Identifier: MIT */
/* expect: 45 */
double mix(int a, double b, int c, double d, int e, double f, int g, double h) { return a+b+c+d+e+f+g+h; }
int main(void) { return (int)mix(1,2,3,4,5,6,7,17); }
