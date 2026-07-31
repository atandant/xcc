/* SPDX-License-Identifier: MIT */
/* expect: 9 */
typedef int I;
int main(void) { auto I x = 4; I auto y = 5; return x + y; }
