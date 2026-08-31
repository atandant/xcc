/* SPDX-License-Identifier: MIT */
/* expect: 76 */
int main(void) { return L"\n\x1234\101"[0] + (L"\n\x1234\101"[1] == 0x1234) + L"\n\x1234\101"[2]; }
