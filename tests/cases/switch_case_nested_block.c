/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    int result;
    result = 0;
    switch (2) {
        {
        case 2:
            result = 7;
            break;
        }
    }
    return result;
}
