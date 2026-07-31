/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of register object 'a' */
int main(void) { register int a[2]; return sizeof(&a); }
