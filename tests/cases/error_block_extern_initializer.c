/* SPDX-License-Identifier: MIT */
/* expect-error: block-scope extern object 'value' cannot have an initializer */
int main(void) { extern int value = 3; return value; }
