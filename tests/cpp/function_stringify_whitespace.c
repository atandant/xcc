/* SPDX-License-Identifier: MIT */
/* expect: 6 */
#define STRING(x) #x
int main(void) { return sizeof(STRING(a   +/**/b)); }
