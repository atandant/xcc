/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) { return (int)(1. + .5 + 2e0 + 3E+0 + 4e-1 + 8.1F); }
