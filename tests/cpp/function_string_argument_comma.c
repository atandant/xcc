/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define FIRST(a, b) a
int main(void) { return FIRST("a,b", "x")[1] == ','; }
