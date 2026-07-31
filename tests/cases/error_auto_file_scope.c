/* SPDX-License-Identifier: MIT */
/* expect-error: invalid storage class for file-scope object 'x' */
auto int x;
int main(void) { return 0; }
