/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) { long i = 0; long s = 0; while (i < 4) { s = s + i; i = i + 1; } return s; }
