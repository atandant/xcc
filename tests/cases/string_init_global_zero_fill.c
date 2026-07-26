/* SPDX-License-Identifier: MIT */
/* expect: 0 */
char text[6] = "abc";
int main(void) { return text[3] + text[5]; }
