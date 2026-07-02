/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) { char a[10]; char *p; char *q; p = a + 7; q = a + 2; return p - q; }
