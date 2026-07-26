/* SPDX-License-Identifier: MIT */
/* expect: 81 */
int first(char *p) { return p[0]; }
int main(void) { return first("Q"); }
