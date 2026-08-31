/* SPDX-License-Identifier: MIT */
/* expect: 102 */
#include <stddef.h>
int main(void) { wchar_t s[] = L"AZ"; return sizeof(s) + s[1]; }
