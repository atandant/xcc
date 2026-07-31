/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { static int values[] = { 2, 4, 6 }; return values[2] - 6; }
