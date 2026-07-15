/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int while_never_runs(void)
{
    int x;

    x = 7;
    while (0) {
        continue;
        x = 1;
    }
    return x - 7;
}

int main(void)
{
    return while_never_runs();
}
