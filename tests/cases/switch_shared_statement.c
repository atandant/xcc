/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    switch (2) {
    case 1:
    case 2:
        return 7;
    default:
        return 9;
    }
}
