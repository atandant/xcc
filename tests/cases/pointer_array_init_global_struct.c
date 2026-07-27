/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int first[2] = { 3, 4 };
int second[2] = { 5, 6 };
struct Table { int (*rows[2])[2]; };
struct Table table = { { { &second }, { &first } } };
int main(void) { return (*table.rows[0])[1] + (*table.rows[1])[0]; }
