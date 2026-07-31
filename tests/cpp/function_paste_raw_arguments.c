/* SPDX-License-Identifier: MIT */
/* expect: 16 */
#define LEFT wrong
#define RIGHT wrong
#define LEFTRIGHT 16
#define CAT(a, b) a ## b
int main(void) { return CAT(LEFT, RIGHT); }
