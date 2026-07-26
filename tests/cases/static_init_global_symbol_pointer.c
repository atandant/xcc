/* SPDX-License-Identifier: MIT */
/* expect: 37 */
int target = 37;
int *selected = &target;
int main(void) { return *selected; }
