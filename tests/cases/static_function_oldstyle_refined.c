/* SPDX-License-Identifier: MIT */
/* expect: 29 */
static int identity();
static int identity(int value);
static int identity(int value) { return value; }
int main(void) { return identity(29); }
