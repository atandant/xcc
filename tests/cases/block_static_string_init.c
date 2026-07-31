/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { static char text[] = "hello"; return text[4] - 111; }
