/* SPDX-License-Identifier: MIT */
/* expect: 8 */
#define TWICE(x) ((x) + (x))
int main(void) { return TWICE(4); }
