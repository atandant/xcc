/* SPDX-License-Identifier: MIT */
/* expect: 19 */
#define VALUE 19
#define CAT(a, b) a ## b
int main(void) { return CAT(VALUE, ); }
