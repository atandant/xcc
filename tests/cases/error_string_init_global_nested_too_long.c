/* SPDX-License-Identifier: MIT */
/* expect-error: character string literal is too long for array */
char text[2][2] = { "abc", "c" };
