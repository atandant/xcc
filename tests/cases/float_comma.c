/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) { double d = 1.0; return (int)(d = 3.0, d += 5.0, d); }
