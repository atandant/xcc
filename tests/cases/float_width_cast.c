/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { double d = 16777217.0; float f = (float)d; return (double)f == 16777216.0; }
