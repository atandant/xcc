/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int A = 9;
#define A B + 1
#define B A + 1
int main(void) { return A; }
