/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { double p = 7.9; float n = -14.2f; return (int)p + (int)n + 256 == 249 && (int)7.9 + 1 == 8; }
