/* SPDX-License-Identifier: MIT */
/* expect: 17 */
#define A X
#define B Y
#define AB 17
#define JOIN A ## B
int main(void) { return JOIN; }
