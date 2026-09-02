/* SPDX-License-Identifier: MIT */
/* expect: 12 */
static int *pointer = (int *)12;
int main(void) { return (long)pointer; }
