/* SPDX-License-Identifier: MIT */
/* expect: 100 */
#line 700 "virtual.c"
int main(void) { return (sizeof(__FILE__) == sizeof("virtual.c")) * 100 + __LINE__ - 700; }
