/* SPDX-License-Identifier: MIT */
/* expect: 37 */
volatile int value;
int main(void)
{
    value = 37;
    return value;
}
