/* SPDX-License-Identifier: MIT */
/* expect: 3 */
#define PRESENT
#ifdef PRESENT
#ifndef MISSING
int main(void) { return 3; }
#endif
#endif
