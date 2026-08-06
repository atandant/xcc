/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'f' */
int f(int);
int f(int, ...);
