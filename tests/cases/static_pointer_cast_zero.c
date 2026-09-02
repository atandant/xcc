/* SPDX-License-Identifier: MIT */
/* expect: 11 */
static int *pointer = (int *)0;
int main(void) { return pointer == 0 ? 11 : 1; }
