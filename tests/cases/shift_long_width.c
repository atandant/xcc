/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) { long x; x = 1L << 40; return (int)(x >> 38); }
