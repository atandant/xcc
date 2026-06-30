/* SPDX-License-Identifier: MIT */
/* expect: 5 */
void set(int *p) { *p = 5; }
int main(void) { int x; set(&x); return x; }
