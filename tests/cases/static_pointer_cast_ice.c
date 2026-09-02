/* SPDX-License-Identifier: MIT */
/* expect: 13 */
static char *pointer = (char *)(6 + 7);
int main(void) { return (long)pointer; }
