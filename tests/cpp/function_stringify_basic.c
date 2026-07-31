/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define STRING(x) #x
int main(void) { return STRING(word)[0] == 'w'; }
