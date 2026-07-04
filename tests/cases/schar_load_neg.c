/* SPDX-License-Identifier: MIT */
/* expect: 255 */
int main(void) { signed char c; c = -1; return (int)(unsigned char)c; }
