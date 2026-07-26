/* SPDX-License-Identifier: MIT */
/* expect: 225 */
struct Name { char text[5]; int value; };
struct Name names[2] = { { "cat", 8 }, { "dog", 9 } };
int main(void) { return names[0].text[2] + names[1].text[0] + names[1].value; }
