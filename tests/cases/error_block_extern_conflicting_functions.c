/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'value' */
void first(void) { extern int value; }
void second(void) { extern long value; }
