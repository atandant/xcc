/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void)
{
    int value;

    value = 2;
    switch (value) {
    case 1:
        return 1;
    case 2:
        goto selected;
    default:
        return 3;
    }
selected:
    return 12;
}
