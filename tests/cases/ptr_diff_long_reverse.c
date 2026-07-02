/* SPDX-License-Identifier: MIT */
/* expect: 254 */
int main(void) { int a[5]; return a - (a + 2); }
