/* SPDX-License-Identifier: MIT */
/* expect-error: condition has non-scalar type 'void' */
void g(void);
int main(void) { if (g()) return 1; return 0; }
