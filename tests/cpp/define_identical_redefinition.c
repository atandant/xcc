/* SPDX-License-Identifier: MIT */
/* expect: 11 */
#define VALUE 5 + 6
#define VALUE 5   +   6
int main(void) { return VALUE; }
