/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) { int a[6]; long d; d = (a + 4) - (a + 1); return d; }
