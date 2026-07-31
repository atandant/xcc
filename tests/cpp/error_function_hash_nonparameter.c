/* SPDX-License-Identifier: MIT */
/* expect-error: '#' is not followed by a macro parameter */
#define BAD(x) # other
int main(void) { return 0; }
