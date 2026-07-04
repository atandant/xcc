/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int a[2][3]; return (a + 1) > a; }
