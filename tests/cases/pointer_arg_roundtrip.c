/* SPDX-License-Identifier: MIT */
/* expect: 5 */
void set5(int *p) { *p = 5; }
int main(void) { int x; set5(&x); return x; }
