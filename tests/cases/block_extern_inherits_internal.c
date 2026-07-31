/* SPDX-License-Identifier: MIT */
/* expect: 0 */
static int value = 13;
int main(void) { extern int value; return value - 13; }
