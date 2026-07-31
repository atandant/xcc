/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int main(void) { int i = -7; unsigned int u = 49u; return (int)((float)i + (double)u); }
