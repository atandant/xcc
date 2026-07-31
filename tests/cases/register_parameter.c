/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int twice(register int x) { return x * 2; }
int main(void) { return twice(6); }
