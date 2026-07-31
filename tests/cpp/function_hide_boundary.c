/* SPDX-License-Identifier: MIT */
/* expect: 14 */
int F(int x) { return x; }
#define F(x) x
#define G F
int main(void) { return G(G)(14); }
