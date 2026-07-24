/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int values[3];
int read_second(int *p) { return p[1]; }
int main(void) { values[1] = 7; return read_second(values); }
