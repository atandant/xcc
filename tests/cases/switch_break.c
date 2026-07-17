/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void)
{
    int result;
    result = 0;
    switch (2) {
    case 2:
        result = 3;
        break;
    default:
        result = 9;
    }
    return result;
}
