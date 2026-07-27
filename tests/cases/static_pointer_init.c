/* SPDX-License-Identifier: MIT */
/* expect: 37 */
static int target = 37;
static int *p = &target;
int main(void) { return *p; }
