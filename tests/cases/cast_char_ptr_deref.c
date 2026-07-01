/* SPDX-License-Identifier: MIT */
/* expect: 200 */
int main(void) { char c; char *p; c = 200; p = &c; return (int)*p; }
