/* SPDX-License-Identifier: MIT */
/* expect: 13 */
static unsigned const long first = 6;
static const unsigned long second = 7;

int main(void)
{
    return first + second;
}
