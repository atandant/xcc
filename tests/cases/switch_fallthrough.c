/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int result;
    result = 0;
    switch (2) {
    case 1:
        result = result + 1;
    case 2:
        result = result + 2;
    case 3:
        result = result + 4;
    }
    return result;
}
