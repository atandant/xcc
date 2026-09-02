/* SPDX-License-Identifier: MIT */
/* expect: 16 */
typedef int Number;
typedef volatile Number VolatileNumber;
int main(void) { VolatileNumber value = 16; return value; }
