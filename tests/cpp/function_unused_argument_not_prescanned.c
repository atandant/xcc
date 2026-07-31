/* SPDX-License-Identifier: MIT */
/* expect: 24 */
#define BAD(x) x
#define IGNORE(x) 24
int main(void) { return IGNORE(BAD(1, 2)); }
