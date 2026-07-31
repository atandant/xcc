/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int value = 9;
int main(void) { static int value = 3; return value - 3; }
