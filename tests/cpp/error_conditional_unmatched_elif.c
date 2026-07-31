/* SPDX-License-Identifier: MIT */
/* expect-error: #elif without matching #if */
#elif 1
int main(void) { return 0; }
