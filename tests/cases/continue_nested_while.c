/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int nested_while_continue(void)
{
    int i;
    int j;
    int total;

    total = 0;
    i = 0;
    while (i < 2) {
        j = 0;
        while (j < 3) {
            j = j + 1;
            if (j == 2)
                continue;
            total = total + 1;
        }
        i = i + 1;
    }
    return total;
}

int main(void)
{
    return nested_while_continue();
}
