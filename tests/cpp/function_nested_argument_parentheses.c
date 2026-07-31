/* SPDX-License-Identifier: MIT */
/* expect: 9 */
#define SECOND(a, b) b
int main(void) { return SECOND((1, 2), 9); }
