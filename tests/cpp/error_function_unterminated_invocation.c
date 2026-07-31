/* SPDX-License-Identifier: MIT */
/* expect-error: unterminated macro invocation */
#define ID(x) x
int main(void) { return ID(1;
