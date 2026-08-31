/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { return L"é"[0] == 0xe9 && L"é"[1] == 0; }
