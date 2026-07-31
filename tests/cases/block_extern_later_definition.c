/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int read(void) { extern int value; return value; }
int value = 17;
int main(void) { return read() - 17; }
