/* SPDX-License-Identifier: MIT */
/* expect: 11 */
#define VALUE 11
#define ID(x) x
int main(void) { return ID(VALUE); }
