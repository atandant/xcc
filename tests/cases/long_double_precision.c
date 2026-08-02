/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double n = 9007199254740992.0L;
    return (n + 1.0L) - n != 1.0L;
}
