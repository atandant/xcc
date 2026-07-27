/* SPDX-License-Identifier: MIT */
/* expect: 17 */
static int value(void);
int value(void) { return 17; }
int main(void) { return value(); }
