/* SPDX-License-Identifier: MIT */
/* expect: 60 */
#line 50 "one.c"
int f(void) { return __LINE__; }
#line 10
int main(void) { return f() + __LINE__; }
