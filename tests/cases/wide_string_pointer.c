/* SPDX-License-Identifier: MIT */
/* expect: 121 */
#include <stddef.h>
int main(void) { wchar_t *p = L"xyz"; return *(p + 1); }
