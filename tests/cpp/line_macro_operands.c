/* SPDX-License-Identifier: MIT */
/* expect: 2 */
#define LINE_NUMBER 123
#define FILE_NAME "mapped.c"
#line LINE_NUMBER FILE_NAME
int main(void) { return (__LINE__ == 123) + (sizeof(__FILE__) == sizeof("mapped.c")); }
