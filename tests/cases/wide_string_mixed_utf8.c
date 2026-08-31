/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int main(void) { return (("é" L"!")[0] == 0xe9) + (("é" L"!")[1] == '!'); }
