/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int main(void) { int a[6]; a[3] = 42; return *(a + 3L); }
