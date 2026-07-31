/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int ITEM(void) { return 8; }
#define ITEM ITEM
int main(void) { return ITEM(); }
