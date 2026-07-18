/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { unsigned char x; x = 128; return (x << 1) >> 8; }
