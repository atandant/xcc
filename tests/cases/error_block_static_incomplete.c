/* SPDX-License-Identifier: MIT */
/* expect-error: block-scope static object 'values' has incomplete type 'int[0]' */
int main(void) { static int values[]; return 0; }
