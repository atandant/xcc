/* SPDX-License-Identifier: MIT */
/* expect: 44 */
int trunc(int x) { return (int)(char)x; }
int main(void) { return trunc(300); }
