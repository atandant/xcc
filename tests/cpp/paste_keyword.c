/* SPDX-License-Identifier: MIT */
/* expect: 22 */
#define GIVE ret ## urn
int main(void) { GIVE 22; }
