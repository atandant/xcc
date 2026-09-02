/* SPDX-License-Identifier: MIT */
/* expect: 19 */
typedef int Number;
int main(void) { register const Number value = 19; return value; }
