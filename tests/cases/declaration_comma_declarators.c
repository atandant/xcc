/* SPDX-License-Identifier: MIT */
/* expect: 0 */

int helper(void) { return 9; }

int main(void)
{
    int values[3] = { 1, 2, 3 }, *p = values, (*arrayp)[3] = &values;
    int helper(void), (*fn)(void) = helper;

    return p[1] == 2 && (*arrayp)[2] == 3 && fn() == 9 ? 0 : 1;
}
