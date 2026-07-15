/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int break_on_entry(void)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < 10; i = i + 1) {
        if (i == 0)
            break;
        total = total + i;
    }
    return total;
}

int main(void)
{
    return break_on_entry();
}
