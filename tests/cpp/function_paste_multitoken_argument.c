/* SPDX-License-Identifier: MIT */
/* expect: 7 */
#define xy 6
#define CAT(a, b) a ## b
int main(void) { return CAT(1 + x, y); }
