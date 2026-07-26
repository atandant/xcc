/* SPDX-License-Identifier: MIT */
/* expect: 99 */
int main(void) { char (*p)[4]; p = &"abc"; return (*p)[2]; }
