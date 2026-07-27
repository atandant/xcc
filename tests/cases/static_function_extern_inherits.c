/* SPDX-License-Identifier: MIT */
/* expect: 23 */
static int value(void);
extern int value(void);
int value(void) { return 23; }
int main(void) { return value(); }
