/* SPDX-License-Identifier: MIT */
/* expect: 20 */
typedef int Number;
int main(void) { register volatile Number value = 20; return value; }
