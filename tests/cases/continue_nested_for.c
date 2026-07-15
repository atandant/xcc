/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int nested_continue_count(void)
{
    int i;
    int j;
    int total;

    total = 0;
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            if (j == 1)
                continue;
            total = total + 1;
        }
    }
    return total;
}

int main(void)
{
    return nested_continue_count();
}
