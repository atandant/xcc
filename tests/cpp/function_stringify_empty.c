/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define STRING(x) #x
int main(void) { return sizeof(STRING()); }
