/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) { long i; long s = 0; for (i = 1; i <= 4; i = i + 1) s = s + i; return s; }
