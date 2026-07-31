/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define STRING(x) #x
#define PASS(x) STRING(a x)
int main(void) { return sizeof(PASS(b)) == 4; }
