/* SPDX-License-Identifier: MIT */
/* expect: 9 */
typedef int I;
int main(void) { register I x = 4; I register y = 5; return x + y; }
