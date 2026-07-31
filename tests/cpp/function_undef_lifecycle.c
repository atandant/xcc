/* SPDX-License-Identifier: MIT */
/* expect: 7 */
#define F(x) x
int before(void) { return F(2); }
#undef F
int F(int x) { return x + 3; }
int main(void) { return before() + F(2); }
