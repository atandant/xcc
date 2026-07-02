/* SPDX-License-Identifier: MIT */
/* expect: 100 */
int main(void) { long x; long y; x = 100L; y = (long)(int)x; return y; }
