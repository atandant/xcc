/* SPDX-License-Identifier: MIT */
/* expect: 31 */
static int value(void) { return 31; }
int main(void)
{
    int (*fn)(void) = value;
    return fn();
}
