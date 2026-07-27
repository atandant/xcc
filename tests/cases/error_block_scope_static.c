/* SPDX-License-Identifier: MIT */
/* expect-error: block-scope storage classes are not yet supported */
int main(void) { static int x = 3; return x; }
