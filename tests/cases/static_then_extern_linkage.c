/* SPDX-License-Identifier: MIT */
/* expect: 1 */
static int x = 1;
extern int x;
int main(void) { return x; }
