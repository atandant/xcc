/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int first[2] = { 3, 4 };
int second[2] = { 5, 6 };
int (*rows[2])[2] = { { &first }, { &second } };
int main(void) { return (*rows[0])[1] + (*rows[1])[0]; }
