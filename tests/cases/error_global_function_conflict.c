/* SPDX-License-Identifier: MIT */
/* expect-error: redeclared 'item' as different kind of symbol */
int item;
int item(void);
