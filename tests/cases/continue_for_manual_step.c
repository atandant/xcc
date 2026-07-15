/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int for_manual_step_break(void)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < 10; ) {
        if (i == 3)
            break;
        total = total + i;
        i = i + 1;
    }
    return total;
}

int main(void)
{
    return for_manual_step_break();
}
