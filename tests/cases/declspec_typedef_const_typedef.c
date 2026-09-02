/* SPDX-License-Identifier: MIT */
/* expect: 15 */
typedef int Number;
typedef const Number Constant;
int main(void) { Constant value = 15; return value; }
