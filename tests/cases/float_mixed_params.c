/* SPDX-License-Identifier: MIT */
/* expect: 36 */
double mix(int a, double b, long c, float d, int e) { return a + b + c + d + e; }
int main(void) { return (int)mix(1, 2.0, 4L, 8.0f, 21); }
