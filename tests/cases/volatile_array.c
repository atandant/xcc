/* SPDX-License-Identifier: MIT */
/* expect: 30 */
int main(void)
{
    volatile int values[3];
    values[0] = 5;
    values[1] = 10;
    values[2] = 15;
    return values[0] + values[1] + values[2];
}
