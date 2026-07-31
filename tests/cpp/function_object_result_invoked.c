/* SPDX-License-Identifier: MIT */
/* expect: 12 */
#define ID(x) x
#define APPLY ID
int main(void) { return APPLY(12); }
