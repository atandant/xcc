/* SPDX-License-Identifier: MIT */
/* expect: 55 */
long id(long x) { return x; }
int main(void) { return id(55L); }
