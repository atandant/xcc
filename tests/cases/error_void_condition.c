/* SPDX-License-Identifier: MIT */
/* expect-error: non-scalar type 'void' */
void g(void);
int main(void) { if (g()) return 1; return 0; }
