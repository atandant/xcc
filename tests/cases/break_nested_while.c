/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int nested_while_break(void)
{
    int i;
    int j;
    int total;

    total = 0;
    i = 0;
    while (i < 3) {
        j = 0;
        while (j < 3) {
            if (j == 1)
                break;
            total = total + 1;
            j = j + 1;
        }
        i = i + 1;
    }
    return total;
}

int main(void)
{
    return nested_while_break();
}
