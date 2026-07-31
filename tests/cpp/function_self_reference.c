/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int SELF(int x) { return x; }
#define SELF(x) SELF(x)
int main(void) { return SELF(13); }
