/* SPDX-License-Identifier: MIT */
/* expect: 7 */
typedef int Value;

int main(void)
{
    {
        int Value;
        Value = 3;
    }
    {
        Value x;
        x = 7;
        return x;
    }
}
