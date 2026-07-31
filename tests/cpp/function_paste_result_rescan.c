/* SPDX-License-Identifier: MIT */
/* expect: 17 */
#define RESULT 17
#define CAT(a, b) a ## b
int main(void) { return CAT(RE, SULT); }
