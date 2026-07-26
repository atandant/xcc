/* SPDX-License-Identifier: MIT */
/* expect: 230 */
char *names[] = { "one", "two" };
int main(void) { return names[0][0] + names[1][1]; }
