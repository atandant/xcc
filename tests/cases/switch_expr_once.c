/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int count;
    int result;
    count = 0;
    result = 0;
    switch (count = count + 1) {
    case 1:
        result = 5;
        break;
    default:
        result = 9;
    }
    return count + result;
}
