/* SPDX-License-Identifier: MIT */
/* expect: 42 */
short dbl(short x) { return x + x; }
int main(void) { return dbl(21); }
