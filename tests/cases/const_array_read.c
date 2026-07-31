/* SPDX-License-Identifier: MIT */
/* expect: 23 */
int main(void)
{
    const int values[3] = { 4, 7, 12 };
    return values[0] + values[1] + values[2];
}
