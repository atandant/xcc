/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int next(void) { static int value = 4; return ++value; }
int main(void) { next(); return next() - 6; }
