/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int read(void) { extern int values[]; return values[1]; }
int values[2] = { 3, 9 };
int main(void) { return read() - 9; }
