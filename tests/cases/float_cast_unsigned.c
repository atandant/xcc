/* SPDX-License-Identifier: MIT */
/* expect: 1 */
unsigned long high = 9223372036854775808UL;
double high_as_double = 9223372036854775808UL;
int main(void) { unsigned int u = 4000000200u; double d = (double)u; double h = (double)high; unsigned long back = (unsigned long)h; return (unsigned int)d - 4000000000u == 200u && h == high_as_double && back == high; }
