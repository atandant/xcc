/* SPDX-License-Identifier: MIT */
/* expect: 0 */
static int g = 10;
int add(void) { return g + 1; }
int main(void) { return add() - 11; }
