/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types initializing 'int[2] *' with 'int[3] *' */
int values[3];
int (*row)[2] = { &values };
