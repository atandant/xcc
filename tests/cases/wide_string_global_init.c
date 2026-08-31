/* SPDX-License-Identifier: MIT */
/* expect: 65 */
#include <stddef.h>
wchar_t text[] = L"A";
int main(void) { return text[0] + text[1]; }
