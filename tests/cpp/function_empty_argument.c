/* SPDX-License-Identifier: MIT */
/* expect: 10 */
#define SECOND(a, b) b
int main(void) { return SECOND(, 10); }
