/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { unsigned int x; x = 0x80000000; return x >> 31; }
