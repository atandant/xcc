/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define EQUAL = ## =
int main(void) { return 4 EQUAL 4; }
