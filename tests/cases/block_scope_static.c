/* SPDX-License-Identifier: MIT */
/* A block static has static storage duration but no linkage. */
/* expect: 3 */
int main(void) { static int x = 3; return x; }
