/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void)
{
    int i;
    int count;
    count = 0;
    for (i = 0; i < 3; i = i + 1) {
        switch (i) {
        case 1:
            break;
        default:
            count = count + 1;
        }
        count = count + 1;
    }
    return count - 2;
}
