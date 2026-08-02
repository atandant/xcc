/* SPDX-License-Identifier: MIT */
/* expect: 0 */
struct S { char a; long double value; char b; };
int main(void) { return sizeof(long double) != 16 || sizeof(struct S) != 48; }
