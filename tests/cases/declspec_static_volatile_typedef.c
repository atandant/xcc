/* SPDX-License-Identifier: MIT */
/* expect: 14 */
typedef int Number;
static volatile Number value = 14;
int main(void) { return value; }
