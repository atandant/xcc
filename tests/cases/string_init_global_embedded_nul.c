/* SPDX-License-Identifier: MIT */
/* expect: 102 */
char text[] = "a\0b";
int main(void) { return sizeof(text) + text[1] + text[2]; }
