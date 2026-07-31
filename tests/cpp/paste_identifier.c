/* SPDX-License-Identifier: MIT */
/* expect: 17 */
int joined = 17;
#define NAME join ## ed
int main(void) { return NAME; }
