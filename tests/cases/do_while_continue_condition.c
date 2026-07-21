/* SPDX-License-Identifier: MIT */
/* expect: 33 */
int main(void)
{
    int i;
    int checks;

    i = 0;
    checks = 0;
    do {
        i = i + 1;
        if (i < 3)
            continue;
    } while ((checks = checks + 1) < 3);
    return i * 10 + checks;
}
