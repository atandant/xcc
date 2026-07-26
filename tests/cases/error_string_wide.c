/* SPDX-License-Identifier: MIT */
/* expect-error: wide string literals are not yet supported */
int main(void) { return L"wide"[0]; }
