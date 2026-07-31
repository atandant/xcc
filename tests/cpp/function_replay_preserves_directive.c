/* SPDX-License-Identifier: MIT */
/* expect: 26 */
#define ID(x) x
int ID = 2;
int before(void) { return ID
#define VALUE 24
; }
int main(void) { return before() + VALUE; }
