/* SPDX-License-Identifier: MIT */
/* expect: 5 */
#define INC(x) ((x) + 1)
int main(void) { return INC(INC(3)); }
