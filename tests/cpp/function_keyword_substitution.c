/* SPDX-License-Identifier: MIT */
/* expect: 23 */
#define DECL(type, name) type name
int main(void) { DECL(int, value) = 23; return value; }
