/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define VALUE 123
#define STRING(x) #x
int main(void) { return STRING(VALUE)[0] == 'V'; }
