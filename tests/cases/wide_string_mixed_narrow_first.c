/* SPDX-License-Identifier: MIT */
/* expect: 197 */
int main(void) { return ("ab" L"cd")[0] + ("ab" L"cd")[3]; }
