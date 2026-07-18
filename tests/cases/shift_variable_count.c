/* SPDX-License-Identifier: MIT */
/* expect: 96 */
int shift(int x, int n) { return x << n; }
int main(void) { return shift(3, 5); }
