/* SPDX-License-Identifier: MIT */
/* expect-error: operand of cast has non-scalar type 'void' */
int main(void) { return (int)(void)5; }
