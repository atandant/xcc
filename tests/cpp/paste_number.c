/* SPDX-License-Identifier: MIT */
/* expect: 123 */
#define NUMBER 1 ## 2 ## 3
int main(void) { return NUMBER; }
