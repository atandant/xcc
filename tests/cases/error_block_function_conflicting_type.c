/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'helper' */
int first(void) { int helper(int); return 0; }
int second(void) { long helper(int); return 0; }
