/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int value = 6;
int main(void) { extern int value; extern int value; return value - 6; }
