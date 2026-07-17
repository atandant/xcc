/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void)
{
    int i;
    int count;
    count = 0;
    for (i = 0; i < 5; i = i + 1) {
        switch (i) {
        case 2:
            continue;
        default:
            break;
        }
        count = count + 1;
    }
    return count;
}
