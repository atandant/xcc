/* SPDX-License-Identifier: MIT */
/* expect: 21 */
#define ABC 21
#define BUILD A ## B ## C
int main(void) { return BUILD; }
