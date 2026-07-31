/* SPDX-License-Identifier: MIT */
/* expect: 25 */
int A(int x) { return x; }
#define A(x) B(x)
#define B(x) A(x)
int main(void) { return A(25); }
