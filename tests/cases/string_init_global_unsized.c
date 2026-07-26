/* SPDX-License-Identifier: MIT */
/* expect: 103 */
char text[] = "abc";
int main(void) { return sizeof(text) + text[2]; }
