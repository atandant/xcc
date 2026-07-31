/* SPDX-License-Identifier: MIT */
/* expect: 19 */
#define A B
#define B C
#define C D
#define D 19
int main(void) { return A; }
