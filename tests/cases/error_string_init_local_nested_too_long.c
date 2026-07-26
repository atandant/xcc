/* SPDX-License-Identifier: MIT */
/* expect-error: character string literal is too long for array */
int main(void) { char text[2][2] = {"abc", "c"}; return text[0][0]; }
