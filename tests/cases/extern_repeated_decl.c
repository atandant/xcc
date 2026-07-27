/* SPDX-License-Identifier: MIT */
/* expect: 1 */
extern int x;
extern int x;
int x = 1;
int main(void) { return x; }
