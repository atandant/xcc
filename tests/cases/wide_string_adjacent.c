/* SPDX-License-Identifier: MIT */
/* expect: 197 */
int main(void) { return (L"ab" L"cd")[0] + (L"ab" L"cd")[3]; }
