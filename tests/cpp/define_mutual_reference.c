/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int LEFT(void) { return 9; }
#define LEFT RIGHT
#define RIGHT LEFT
int main(void) { return LEFT(); }
