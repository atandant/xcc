/* SPDX-License-Identifier: MIT */
/* expect: 20 */
int main(void)
{
    int value;
    value = 2;
    switch (value) {
    case 1:
        return 10;
    case 2:
        return 20;
    default:
        return 30;
    }
}
