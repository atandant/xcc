/* SPDX-License-Identifier: MIT */
/* expect-error: static function 'value' is used but never defined */
static int value(void);
int main(void) { return value(); }
