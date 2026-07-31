/* SPDX-License-Identifier: MIT */
/* expect: 27 */
#define xyz 27
#define CAT3(a, b, c) a ## b ## c
int main(void) { return CAT3(x, y, z); }
