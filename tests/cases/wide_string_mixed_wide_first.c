/* SPDX-License-Identifier: MIT */
/* expect: 197 */
int main(void) { return (L"ab" "cd")[0] + (L"ab" "cd")[3]; }
