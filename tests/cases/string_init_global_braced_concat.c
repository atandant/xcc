/* SPDX-License-Identifier: MIT */
/* expect: 3 */
char text[] = { "A" "\102" };
int main(void) { return sizeof(text) + text[2]; }
