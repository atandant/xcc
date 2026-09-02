/* SPDX-License-Identifier: MIT */
/* expect: 18 */
typedef int Number;
int main(void) { auto volatile Number value = 18; return value; }
