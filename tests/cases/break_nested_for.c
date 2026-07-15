/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int nested_break_count(void)
{
    int i;
    int j;
    int total;

    total = 0;
    for (i = 0; i < 5; i = i + 1) {
        for (j = 0; j < 5; j = j + 1) {
            if (j == 2)
                break;
            total = total + 1;
        }
    }
    return total;
}

int main(void)
{
    return nested_break_count();
}
