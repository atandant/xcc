/* SPDX-License-Identifier: MIT */
/* expect: 82 */
int main(void) { return sizeof(L"A\0B") + L"A\0B"[2]; }
