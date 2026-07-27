/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int values[2] = { 3, 4 };
int (*rows[2])[2] = { { &values } };
int main(void) { return rows[0] != 0 && rows[1] == 0; }
