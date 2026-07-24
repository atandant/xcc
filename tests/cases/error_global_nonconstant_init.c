/* SPDX-License-Identifier: MIT */
/* expect-error: initializer for file-scope object 'value' is not constant */
int make(void) { return 3; }
int value = make();
