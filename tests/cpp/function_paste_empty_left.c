/* SPDX-License-Identifier: MIT */
/* expect: 18 */
#define VALUE 18
#define CAT(a, b) a ## b
int main(void) { return CAT(, VALUE); }
