/* SPDX-License-Identifier: MIT */
/* expect: 120 */
char *text = "wxyz" + 3 - 2;
int main(void) { return *text; }
