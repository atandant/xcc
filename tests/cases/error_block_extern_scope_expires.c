/* SPDX-License-Identifier: MIT */
/* expect-error: use of undeclared identifier 'hidden' */
void declare(void) { extern int hidden; }
int main(void) { return hidden; }
