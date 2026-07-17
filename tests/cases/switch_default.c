/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void)
{
    switch (4) {
    case 1:
        return 1;
    default:
        return 9;
    }
}
