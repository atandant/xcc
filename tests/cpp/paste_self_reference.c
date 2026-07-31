/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int LOOP(void) { return 8; }
#define LOOP LO ## OP
int main(void) { return LOOP(); }
