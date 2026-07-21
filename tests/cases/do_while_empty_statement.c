/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    int condition;

    condition = 0;
    do
        ;
    while (condition);
    return 7;
}
