/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int values[3] = { 1, 2, 7 };
int main(void) { extern int values[]; extern int values[3]; return values[2] - 7; }
