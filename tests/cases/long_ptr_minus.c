/* SPDX-License-Identifier: MIT */
/* expect: 17 */
int main(void) { int a[5]; a[2] = 17; return *((a + 3L) - 1L); }
