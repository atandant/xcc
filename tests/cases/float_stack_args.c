/* SPDX-License-Identifier: MIT */
/* expect: 55 */
float sum10(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j) { return a+b+c+d+e+f+g+h+i+j; }
int main(void) { return (int)sum10(1,2,3,4,5,6,7,8,9,10); }
