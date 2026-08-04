/* SPDX-License-Identifier: MIT */
/* expect: 29 */
typedef int Number;
volatile Number value;
int main(void)
{
    Number volatile local;
    value = 12;
    local = 17;
    return value + local;
}
