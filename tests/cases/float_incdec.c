/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) { double d = 4.0; float f = 2.0f; int r = (int)d++ + (int)++d + (int)f-- + (int)--f; return r; }
